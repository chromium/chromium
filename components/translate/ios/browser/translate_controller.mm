// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/translate/ios/browser/translate_controller.h"

#include <cmath>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/string_escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/translate/core/common/translate_util.h"
#import "components/translate/ios/browser/translate_java_script_feature.h"
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

namespace translate {

namespace {

// Extracts a TranslateErrors value from `value` for the given `key`. Returns
// std::nullopt if the value is missing or not convertible to TranslateErrors.
std::optional<TranslateErrors> FindTranslateErrorsKey(
    const base::Value::Dict& value,
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

WEB_STATE_USER_DATA_KEY_IMPL(TranslateController)

TranslateController::TranslateController(web::WebState* web_state)
    : web_state_(web_state), observer_(nullptr), weak_method_factory_(this) {
  DCHECK(web_state_);
  web_state_->AddObserver(this);
  if (web_state_->IsRealized()) {
    TranslateJavaScriptFeature::GetInstance()
        ->GetWebFramesManager(web_state_)
        ->AddObserver(this);
  }
}

TranslateController::~TranslateController() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
    if (web_state_->IsRealized()) {
      TranslateJavaScriptFeature::GetInstance()
          ->GetWebFramesManager(web_state_)
          ->RemoveObserver(this);
    }
    web_state_ = nullptr;
  }
}

void TranslateController::InjectTranslateScript(
    const std::string& translate_script) {
  TranslateJavaScriptFeature::GetInstance()->InjectTranslateScript(
      main_web_frame_, translate_script);
}

void TranslateController::RevertTranslation() {
  TranslateJavaScriptFeature::GetInstance()->RevertTranslation(main_web_frame_);
}

void TranslateController::StartTranslation(const std::string& source_language,
                                           const std::string& target_language) {
  TranslateJavaScriptFeature::GetInstance()->StartTranslation(
      main_web_frame_, source_language, target_language);
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
  }
}

void TranslateController::OnTranslateReady(const base::Value::Dict& payload) {
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
  if (observer_) {
    observer_->OnTranslateScriptReady(*error_type, load_time.value_or(0.),
                                      ready_time.value_or(0.));
  }
}

void TranslateController::OnTranslateComplete(
    const base::Value::Dict& payload) {
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

  if (observer_) {
    observer_->OnTranslateComplete(
        *error_type, source_language ? *source_language : std::string(),
        translation_time.value_or(0.));
  }
}

#pragma mark - web::WebStateObserver implementation

void TranslateController::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  if (web_state_->IsRealized()) {
    TranslateJavaScriptFeature::GetInstance()
        ->GetWebFramesManager(web_state_)
        ->RemoveObserver(this);
  }
  web_state_ = nullptr;
  main_web_frame_ = nullptr;
}

void TranslateController::WebStateRealized(web::WebState* web_state) {
  TranslateJavaScriptFeature::GetInstance()
      ->GetWebFramesManager(web_state_)
      ->AddObserver(this);
}

#pragma mark - web::WebFramesManager implementation

void TranslateController::WebFrameBecameAvailable(
    web::WebFramesManager* web_frames_manager,
    web::WebFrame* web_frame) {
  if (web_frame->IsMainFrame()) {
    main_web_frame_ = web_frame;
  }
}

void TranslateController::WebFrameBecameUnavailable(
    web::WebFramesManager* web_frames_manager,
    const std::string& frame_id) {
  if (web_frames_manager->GetFrameWithId(frame_id) == main_web_frame_) {
    main_web_frame_ = nullptr;
  }
}

}  // namespace translate
