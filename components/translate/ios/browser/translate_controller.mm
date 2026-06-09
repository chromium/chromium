// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/translate/ios/browser/translate_controller.h"

#include <cmath>
#include <optional>
#include <string_view>
#include <utility>

#import "base/check_op.h"
#import "base/debug/dump_without_crashing.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/json/string_escape.h"
#import "base/strings/utf_string_conversions.h"
#import "base/values.h"
#import "components/translate/core/browser/translate_download_manager.h"
#import "components/translate/core/common/translate_util.h"
#import "components/translate/ios/browser/translate_java_script_feature.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "net/base/load_flags.h"
#import "net/base/net_errors.h"
#import "net/http/http_status_code.h"
#import "net/traffic_annotation/network_traffic_annotation.h"
#import "services/network/public/cpp/resource_request.h"
#import "services/network/public/cpp/simple_url_loader.h"
#import "services/network/public/mojom/url_response_head.mojom.h"
#import "url/gurl.h"

namespace translate {

namespace {

// The maximum size of the translate script in bytes.
static constexpr size_t kMaxTranslateScriptSize = 1024 * 1024;

// Extracts a TranslateErrors value from `value` for the given `key`. Returns
// std::nullopt if the value is missing or not convertible to TranslateErrors.
std::optional<TranslateErrors> FindTranslateErrorsKey(
    const base::DictValue& value,
    std::string_view key) {
  // Does `value` contains a double value for `key`?
  const std::optional<double> found_value = value.FindDouble(key);
  if (!found_value.has_value())
    return std::nullopt;

  // Does the double value convert to an integral value? This is to reject
  // values like `1.3` that do not represent an enumerator.
  const double double_value = found_value.value();
  if (double_value != std::trunc(double_value))
    return std::nullopt;

  // Is the value in range? It is safe to convert the enumerator values to
  // `double` as IEEE754 floating point can safely represent all integral
  // values below 2**53 as they have 53 bits for the mantissa.
  constexpr double kMinValue = static_cast<double>(TranslateErrors::NONE);
  constexpr double kMaxValue = static_cast<double>(TranslateErrors::TYPE_LAST);
  if (double_value < kMinValue || kMaxValue < double_value)
    return std::nullopt;

  // Since `double_value` has no fractional part, is in range for the
  // enumeration and the enumeration has no holes between enumerators,
  // it is safe to cast the value to the enumeration.
  return static_cast<TranslateErrors>(double_value);
}

}  // anonymous namespace

TranslateController::TranslateController(web::WebState* web_state)
    : web_state_(web_state) {
  DCHECK(web_state_);
}

TranslateController::~TranslateController() {
  for (Observer& observer : observers_) {
    observer.TranslateControllerWasDestroyed(this);
  }
}

void TranslateController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TranslateController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

web::WebFrame* TranslateController::GetMainWebFrame() {
  return TranslateJavaScriptFeature::GetInstance()
      ->GetWebFramesManager(web_state_)
      ->GetMainWebFrame();
}

void TranslateController::InjectTranslateScript(
    const std::string& translate_script) {
  web::WebFrame* main_web_frame = GetMainWebFrame();
  if (main_web_frame) {
    translate_script_injected_frame_id_ = main_web_frame->GetFrameId();
    TranslateJavaScriptFeature::GetInstance()->InjectTranslateScript(
        main_web_frame, translate_script);
  }
}

void TranslateController::RevertTranslation() {
  web::WebFrame* main_web_frame = GetMainWebFrame();
  if (main_web_frame) {
    TranslateJavaScriptFeature::GetInstance()->RevertTranslation(
        main_web_frame);
  }
}

void TranslateController::StartTranslation(const std::string& source_language,
                                           const std::string& target_language) {
  web::WebFrame* main_web_frame = GetMainWebFrame();
  if (main_web_frame) {
    TranslateJavaScriptFeature::GetInstance()->StartTranslation(
        main_web_frame, source_language, target_language);
  }
}

void TranslateController::OnJavascriptCommandReceived(
    url::Origin security_origin,
    const base::DictValue& payload) {
  // Ignore messages until translate script is injected
  if (!translate_script_injected_frame_id_.has_value()) {
    return;
  }
  web::WebFrame* main_web_frame = GetMainWebFrame();
  if (!main_web_frame ||
      main_web_frame->GetFrameId() != translate_script_injected_frame_id_) {
    // Web Frame is no longer valid, no need to keep storing this ID.
    translate_script_injected_frame_id_.reset();
    return;
  }

  const std::string* command = payload.FindString("command");
  if (!command) {
    return;
  }

  if (*command == "ready") {
    OnTranslateReady(payload);
  } else if (*command == "status") {
    OnTranslateComplete(payload);
  } else if (*command == "loadJavascript") {
    OnLoadJavascript(security_origin, payload);
  }
}

void TranslateController::OnTranslateReady(const base::DictValue& payload) {
  std::optional<TranslateErrors> error_type =
      FindTranslateErrorsKey(payload, "errorCode");
  if (!error_type.has_value())
    return;

  std::optional<double> load_time;
  std::optional<double> ready_time;
  if (*error_type == TranslateErrors::NONE) {
    load_time = payload.FindDouble("loadTime");
    ready_time = payload.FindDouble("readyTime");
    if (!load_time.has_value() || !ready_time.has_value()) {
      return;
    }
  }

  for (Observer& observer : observers_) {
    observer.OnTranslateScriptReady(*error_type, load_time.value_or(0.),
                                    ready_time.value_or(0.));
  }
}

void TranslateController::OnTranslateComplete(const base::DictValue& payload) {
  std::optional<TranslateErrors> error_type =
      FindTranslateErrorsKey(payload, "errorCode");
  if (!error_type.has_value())
    return;

  const std::string* source_language = nullptr;
  std::optional<double> translation_time;
  if (*error_type == TranslateErrors::NONE) {
    source_language = payload.FindString("pageSourceLanguage");
    translation_time = payload.FindDouble("translationTime");
    if (!source_language || !translation_time.has_value()) {
      return;
    }
  }

  for (Observer& observer : observers_) {
    observer.OnTranslateComplete(
        *error_type, source_language ? *source_language : std::string(),
        translation_time.value_or(0.));
  }
}

void TranslateController::OnLoadJavascript(url::Origin security_origin,
                                           const base::DictValue& payload) {
  const std::string* url_str = payload.FindString("url");
  const std::string* frame_id = payload.FindString("frameId");
  if (!url_str || !frame_id) {
    return;
  }
  GURL url(*url_str);
  if (!url.is_valid()) {
    return;
  }

  // If the TranslateDownloadManager's request context getter is nullptr then
  // shutdown is in progress. Abort the request, which can't proceed with a
  // null url_loader_factory.
  network::mojom::URLLoaderFactory* url_loader_factory =
      TranslateDownloadManager::GetInstance()->url_loader_factory().get();
  if (!url_loader_factory) {
    return;
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation(
          "translate_element_script_url_fetcher", R"(
          semantics {
            sender: "Translate"
            description:
              "If Chrome translation is enabled, downloads script "
              "dependencies needed by the primary translation script."
            trigger:
              "When a page translation is requested on iOS. For more details "
              "about when the primary translation script is fetched, see the "
              "network annotation for `translate_url_fetcher`"
            data: "URL of the script to load."
            destination: GOOGLE_OWNED_SERVICE
          }
        policy {
          cookies_allowed: NO
          setting:
            "Users can enable/disable this feature by toggling 'Offer to "
            "translate pages that aren't in a language you read.' in Chrome "
            "settings under Languages. The list of supported languages is "
            "downloaded regardless of the settings."
          chrome_policy {
            TranslateEnabled {
              TranslateEnabled: false
            }
          }
          })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  script_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                    traffic_annotation);

  script_loader_->DownloadToString(
      url_loader_factory,
      base::BindOnce(&TranslateController::OnScriptLoaded,
                     base::Unretained(this), *frame_id),
      kMaxTranslateScriptSize);
}

void TranslateController::OnScriptLoaded(
    std::string frame_id,
    std::optional<std::string> response_body) {
  web::WebFrame* main_web_frame = GetMainWebFrame();
  if (main_web_frame && main_web_frame->GetFrameId() == frame_id) {
    std::u16string result;
    if (response_body) {
      result = base::UTF8ToUTF16(*response_body);
    } else {
      if (script_loader_->NetError() == net::ERR_INSUFFICIENT_RESOURCES) {
        // Script is larger than `kMaxTranslateScriptSize`.
        base::debug::DumpWithoutCrashing();
      }

      result =
          u"cr.googleTranslate.onTranslateElementError('Script load error');";
    }
    main_web_frame->ExecuteJavaScript(
        result, base::DoNothingAs<void(const base::Value*, NSError*)>());
  }

  script_loader_.reset();
}

}  // namespace translate
