// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/translate/ios/browser/translate_controller.h"

#include <cmath>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/string_escape.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/translate/core/common/translate_util.h"
#import "components/translate/ios/browser/js_translate_web_frame_manager.h"
#import "components/translate/ios/browser/js_translate_web_frame_manager_factory.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/js_messaging/web_frame.h"
#include "ios/web/public/navigation/navigation_context.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace translate {

namespace {

// Extracts a TranslateErrors value from `value` for the given `key`. Returns
// absl::nullopt if the value is missing or not convertible to TranslateErrors.
absl::optional<TranslateErrors> FindTranslateErrorsKey(
    const base::Value::Dict& value,
    base::StringPiece key) {
  // Does `value` contains a double value for `key`?
  const absl::optional<double> found_value = value.FindDouble(key);
  if (!found_value.has_value())
    return absl::nullopt;

  // Does the double value convert to an integral value? This is to reject
  // values like `1.3` that do not represent an enumerator.
  const double double_value = found_value.value();
  if (double_value != std::trunc(double_value))
    return absl::nullopt;

  // Is the value in range? It is safe to convert the enumerator values to
  // `double` as IEEE754 floating point can safely represent all integral
  // values below 2**53 as they have 53 bits for the mantissa.
  constexpr double kMinValue = static_cast<double>(TranslateErrors::NONE);
  constexpr double kMaxValue = static_cast<double>(TranslateErrors::TYPE_LAST);
  if (double_value < kMinValue || kMaxValue < double_value)
    return absl::nullopt;

  // Since `double_value` has no fractional part, is in range for the
  // enumeration and the enumeration has no holes between enumerators,
  // it is safe to cast the value to the enumeration.
  return static_cast<TranslateErrors>(double_value);
}

}  // anonymous namespace

WEB_STATE_USER_DATA_KEY_IMPL(TranslateController)

TranslateController::TranslateController(
    web::WebState* web_state,
    JSTranslateWebFrameManagerFactory* js_manager_factory)
    : web_state_(web_state),
      observer_(nullptr),
      js_manager_factory_(js_manager_factory),
      weak_method_factory_(this) {
  DCHECK(web_state_);
  web_state_->AddObserver(this);
  if (web_state_->IsRealized()) {
    web_state_->GetWebFramesManager()->AddObserver(this);
  }
}

TranslateController::~TranslateController() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_->GetWebFramesManager()->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

void TranslateController::InjectTranslateScript(
    const std::string& translate_script) {
  if (!main_web_frame_) {
    return;
  }

  js_manager_factory_->FromWebFrame(main_web_frame_)
      ->InjectTranslateScript(translate_script);
}

void TranslateController::RevertTranslation() {
  if (!main_web_frame_) {
    return;
  }

  js_manager_factory_->FromWebFrame(main_web_frame_)->RevertTranslation();
}

void TranslateController::StartTranslation(const std::string& source_language,
                                           const std::string& target_language) {
  if (!main_web_frame_) {
    return;
  }

  js_manager_factory_->FromWebFrame(main_web_frame_)
      ->StartTranslation(source_language, target_language);
}

void TranslateController::SetJsTranslateWebFrameManagerFactoryForTesting(
    JSTranslateWebFrameManagerFactory* manager) {
  js_manager_factory_ = manager;
}

void TranslateController::OnJavascriptCommandReceived(
    const base::Value::Dict& payload) {
  const std::string* command = payload.FindString("command");
  if (!command) {
    return;
  }

  if (*command == "ready") {
    OnTranslateReady(payload);
  } else if (*command == "status") {
    OnTranslateComplete(payload);
  } else if (*command == "loadjavascript") {
    OnTranslateLoadJavaScript(payload);
  } else if (*command == "sendrequest") {
    OnTranslateSendRequest(payload);
  }
}

void TranslateController::OnTranslateReady(const base::Value::Dict& payload) {
  absl::optional<TranslateErrors> error_type =
      FindTranslateErrorsKey(payload, "errorCode");
  if (!error_type.has_value())
    return;

  absl::optional<double> load_time;
  absl::optional<double> ready_time;
  if (*error_type == TranslateErrors::NONE) {
    load_time = payload.FindDouble("loadTime");
    ready_time = payload.FindDouble("readyTime");
    if (!load_time.has_value() || !ready_time.has_value()) {
      return;
    }
  }
  if (observer_) {
    observer_->OnTranslateScriptReady(*error_type, load_time.value_or(0.),
                                      ready_time.value_or(0.));
  }
}

void TranslateController::OnTranslateComplete(
    const base::Value::Dict& payload) {
  absl::optional<TranslateErrors> error_type =
      FindTranslateErrorsKey(payload, "errorCode");
  if (!error_type.has_value())
    return;

  const std::string* source_language = nullptr;
  absl::optional<double> translation_time;
  if (*error_type == TranslateErrors::NONE) {
    source_language = payload.FindString("pageSourceLanguage");
    translation_time = payload.FindDouble("translationTime");
    if (!source_language || !translation_time.has_value()) {
      return;
    }
  }

  if (observer_) {
    observer_->OnTranslateComplete(
        *error_type, source_language ? *source_language : std::string(),
        translation_time.value_or(0.));
  }
}

void TranslateController::OnTranslateLoadJavaScript(
    const base::Value::Dict& payload) {
  const std::string* url = payload.FindString("url");
  if (!url) {
    return;
  }

  GURL security_origin = translate::GetTranslateSecurityOrigin();
  if (url->find(security_origin.spec()) || script_fetcher_) {
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(*url);

  script_fetcher_ = network::SimpleURLLoader::Create(
      std::move(resource_request), NO_TRAFFIC_ANNOTATION_YET);
  script_fetcher_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      web_state_->GetBrowserState()->GetURLLoaderFactory(),
      base::BindOnce(&TranslateController::OnScriptFetchComplete,
                     base::Unretained(this)));
}

void TranslateController::OnTranslateSendRequest(
    const base::Value::Dict& payload) {
  const std::string* method = payload.FindString("method");
  const std::string* url = payload.FindString("url");
  const std::string* body = payload.FindString("body");

  if (!method || !url || !body) {
    return;
  }
  absl::optional<double> request_id = payload.FindDouble("requestID");
  if (!request_id.has_value()) {
    return;
  }

  GURL security_origin = translate::GetTranslateSecurityOrigin();
  if (url->find(security_origin.spec())) {
    return;
  }

  auto request = std::make_unique<network::ResourceRequest>();
  request->method = *method;
  request->url = GURL(*url);
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  auto fetcher = network::SimpleURLLoader::Create(std::move(request),
                                                  NO_TRAFFIC_ANNOTATION_YET);
  fetcher->AttachStringForUpload(*body, "application/x-www-form-urlencoded");
  auto* raw_fetcher = fetcher.get();
  auto pair = request_fetchers_.insert(std::move(fetcher));
  raw_fetcher->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      web_state_->GetBrowserState()->GetURLLoaderFactory(),
      base::BindOnce(&TranslateController::OnRequestFetchComplete,
                     base::Unretained(this), pair.first, *url,
                     static_cast<int>(*request_id)));
}

void TranslateController::OnScriptFetchComplete(
    std::unique_ptr<std::string> response_body) {
  if (main_web_frame_ && response_body) {
    main_web_frame_->ExecuteJavaScript(base::UTF8ToUTF16(*response_body));
  }
  script_fetcher_.reset();
}

void TranslateController::OnRequestFetchComplete(
    std::set<std::unique_ptr<network::SimpleURLLoader>>::iterator it,
    std::string url,
    int request_id,
    std::unique_ptr<std::string> response_body) {
  if (!main_web_frame_) {
    return;
  }

  const std::unique_ptr<network::SimpleURLLoader>& url_loader = *it;

  int response_code = 0;
  std::string status_text;
  const network::mojom::URLResponseHead* response_head =
      url_loader->ResponseInfo();
  int net_error_code = url_loader->NetError();

  // |ResponseInfo()| may be a nullptr if response is incomplete.
  if (net_error_code == net::Error::OK && response_head &&
      response_head->headers) {
    net::HttpResponseHeaders* headers = response_head->headers.get();
    response_code = headers->response_code();
    status_text = headers->GetStatusText();
  } else {
    response_code = net::HttpStatusCode::HTTP_BAD_REQUEST;
  }

  // Escape the returned string so it can be parsed by JSON.parse.
  std::string response_text = response_body ? *response_body : "";
  std::string escaped_response_text;
  base::EscapeJSONString(response_text, /*put_in_quotes=*/false,
                         &escaped_response_text);

  std::string final_url = url_loader->GetFinalURL().spec();
  js_manager_factory_->FromWebFrame(main_web_frame_)
      ->HandleTranslateResponse(url, request_id, response_code, status_text,
                                final_url, escaped_response_text);
  request_fetchers_.erase(it);
}

#pragma mark - web::WebStateObserver implementation

void TranslateController::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  if (web_state_->IsRealized()) {
    web_state_->GetWebFramesManager()->RemoveObserver(this);
  }
  web_state_ = nullptr;
  main_web_frame_ = nullptr;

  request_fetchers_.clear();
  script_fetcher_.reset();
}

void TranslateController::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!navigation_context->IsSameDocument()) {
    request_fetchers_.clear();
    script_fetcher_.reset();
  }
}

void TranslateController::WebStateRealized(web::WebState* web_state) {
  web_state_->GetWebFramesManager()->AddObserver(this);
}

#pragma mark - web::WebFramesManager implementation

void TranslateController::WebFrameBecameAvailable(
    web::WebFramesManager* web_frames_manager,
    web::WebFrame* web_frame) {
  DCHECK_EQ(web_state_->GetWebFramesManager(), web_frames_manager);
  if (web_frame->IsMainFrame()) {
    js_manager_factory_->CreateForWebFrame(web_frame);
    main_web_frame_ = web_frame;
  }
}

void TranslateController::WebFrameBecameUnavailable(
    web::WebFramesManager* web_frames_manager,
    const std::string frame_id) {
  DCHECK_EQ(web_state_->GetWebFramesManager(), web_frames_manager);
  if (web_frames_manager->GetFrameWithId(frame_id) == main_web_frame_) {
    main_web_frame_ = nullptr;
  }
}

}  // namespace translate
