// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/translate/ios/browser/translate_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/json/string_escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/translate/core/common/translate_util.h"
#import "components/translate/ios/browser/js_translate_manager.h"
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
// Prefix for the translate javascript commands. Must be kept in sync with
// translate_ios.js.
const char kCommandPrefix[] = "translate";
}

TranslateController::TranslateController(web::WebState* web_state,
                                         JsTranslateManager* manager)
    : web_state_(web_state),
      observer_(nullptr),
      js_manager_(manager),
      weak_method_factory_(this) {
  DCHECK(js_manager_);
  DCHECK(web_state_);
  web_state_->AddObserver(this);
  subscription_ = web_state_->AddScriptCommandCallback(
      base::BindRepeating(
          base::IgnoreResult(&TranslateController::OnJavascriptCommandReceived),
          base::Unretained(this)),
      kCommandPrefix);
}

TranslateController::~TranslateController() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

void TranslateController::InjectTranslateScript(
    const std::string& translate_script) {
  [js_manager_ injectWithTranslateScript:translate_script];
}

void TranslateController::RevertTranslation() {
  [js_manager_ revertTranslation];
}

void TranslateController::StartTranslation(const std::string& source_language,
                                           const std::string& target_language) {
  [js_manager_ startTranslationFrom:source_language to:target_language];
}

void TranslateController::SetJsTranslateManagerForTesting(
    JsTranslateManager* manager) {
  js_manager_ = manager;
}

bool TranslateController::OnJavascriptCommandReceived(
    const base::Value& command,
    const GURL& page_url,
    bool user_is_interacting,
    web::WebFrame* sender_frame) {
  if (!sender_frame->IsMainFrame()) {
    // Translate is only supported on main frame.
    return false;
  }
  const std::string* command_string = command.FindStringKey("command");
  if (!command_string) {
    return false;
  }

  if (*command_string == "translate.ready")
    return OnTranslateReady(command);
  if (*command_string == "translate.status")
    return OnTranslateComplete(command);
  if (*command_string == "translate.loadjavascript")
    return OnTranslateLoadJavaScript(command);
  if (*command_string == "translate.sendrequest")
    return OnTranslateSendRequest(command);

  return false;
}

bool TranslateController::OnTranslateReady(const base::Value& command) {
  absl::optional<double> error_code = command.FindDoubleKey("errorCode");
  if (!error_code.has_value() || *error_code < TranslateErrors::NONE ||
      *error_code >= TranslateErrors::TRANSLATE_ERROR_MAX) {
    return false;
  }

  absl::optional<double> load_time;
  absl::optional<double> ready_time;

  const TranslateErrors::Type error_type =
      static_cast<TranslateErrors::Type>(*error_code);
  if (error_type == TranslateErrors::NONE) {
    load_time = command.FindDoubleKey("loadTime");
    ready_time = command.FindDoubleKey("readyTime");
    if (!load_time.has_value() || !ready_time.has_value()) {
      return false;
    }
  }
  if (observer_) {
    observer_->OnTranslateScriptReady(error_type, load_time.value_or(0.),
                                      ready_time.value_or(0.));
  }
  return true;
}

bool TranslateController::OnTranslateComplete(const base::Value& command) {
  absl::optional<double> error_code = command.FindDoubleKey("errorCode");
  if (!error_code.has_value() || *error_code < TranslateErrors::NONE ||
      *error_code >= TranslateErrors::TRANSLATE_ERROR_MAX) {
    return false;
  }

  const std::string* source_language = nullptr;
  absl::optional<double> translation_time;

  const TranslateErrors::Type error_type =
      static_cast<TranslateErrors::Type>(*error_code);
  if (error_type == TranslateErrors::NONE) {
    source_language = command.FindStringKey("pageSourceLanguage");
    translation_time = command.FindDoubleKey("translationTime");
    if (!source_language || !translation_time.has_value()) {
      return false;
    }
  }

  if (observer_) {
    observer_->OnTranslateComplete(
        error_type, source_language ? *source_language : std::string(),
        translation_time.value_or(0.));
  }
  return true;
}

bool TranslateController::OnTranslateLoadJavaScript(
    const base::Value& command) {
  const std::string* url = command.FindStringKey("url");
  if (!url) {
    return false;
  }

  GURL security_origin = translate::GetTranslateSecurityOrigin();
  if (url->find(security_origin.spec()) || script_fetcher_) {
    return false;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(*url);

  script_fetcher_ = network::SimpleURLLoader::Create(
      std::move(resource_request), NO_TRAFFIC_ANNOTATION_YET);
  script_fetcher_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      web_state_->GetBrowserState()->GetURLLoaderFactory(),
      base::BindOnce(&TranslateController::OnScriptFetchComplete,
                     base::Unretained(this)));

  return true;
}

bool TranslateController::OnTranslateSendRequest(const base::Value& command) {
  const std::string* method = command.FindStringKey("method");
  if (!method) {
    return false;
  }
  const std::string* url = command.FindStringKey("url");
  if (!url) {
    return false;
  }
  const std::string* body = command.FindStringKey("body");
  if (!body) {
    return false;
  }
  absl::optional<double> request_id = command.FindDoubleKey("requestID");
  if (!request_id.has_value()) {
    return false;
  }

  GURL security_origin = translate::GetTranslateSecurityOrigin();
  if (url->find(security_origin.spec())) {
    return false;
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
  return true;
}

void TranslateController::OnScriptFetchComplete(
    std::unique_ptr<std::string> response_body) {
  if (response_body) {
    web_state_->ExecuteJavaScript(base::UTF8ToUTF16(*response_body));
  }
  script_fetcher_.reset();
}

void TranslateController::OnRequestFetchComplete(
    std::set<std::unique_ptr<network::SimpleURLLoader>>::iterator it,
    std::string url,
    int request_id,
    std::unique_ptr<std::string> response_body) {
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
  [js_manager_ handleTranslateResponseWithURL:url
                                    requestID:request_id
                                 responseCode:response_code
                                   statusText:status_text
                                  responseURL:final_url
                                 responseText:escaped_response_text];

  request_fetchers_.erase(it);
}

// web::WebStateObserver implementation.

void TranslateController::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;

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

}  // namespace translate
