// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/ios/browser/ios_language_detection_tab_helper.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/elapsed_timer.h"
#include "components/language/core/browser/url_language_histogram.h"
#include "components/language/ios/browser/language_detection_java_script_feature.h"
#include "components/language/ios/browser/string_clipping_util.h"
#include "components/prefs/pref_member.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_util.h"
#include "components/translate/core/language_detection/language_detection_model.h"
#include "components/translate/core/language_detection/language_detection_util.h"
#import "ios/web/common/url_scheme_util.h"
#include "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/navigation/navigation_context.h"
#include "net/http/http_response_headers.h"

namespace language {

// Note: This should stay in sync with the constant in language_detection.js.
const size_t kMaxIndexChars = 65535;

namespace {
// Name for the UMA metric used to track language detection evaluation duration.
const char kTranslateLanguageDetectionTFLiteModelEvaluationDuration[] =
    "Translate.LanguageDetection.TFLiteModelEvaluationDuration";

// The old CLD model version.
const char kCLDModelVersion[] = "CLD3";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class LanguageDetectionMethod {
  kTFLiteModelUsed = 0,
  kTFLiteModelUnavailable = 1,
  kTFLiteModelDisabled = 2,
  kTFLiteModelIgnored_DEPRECATED = 3,
  kMaxValue = kTFLiteModelIgnored_DEPRECATED
};
}  // namespace

IOSLanguageDetectionTabHelper::IOSLanguageDetectionTabHelper(
    web::WebState* web_state,
    UrlLanguageHistogram* url_language_histogram,
    translate::LanguageDetectionModel* language_detection_model,
    PrefService* prefs)
    : web_state_(web_state),
      url_language_histogram_(url_language_histogram),
      language_detection_model_(language_detection_model),
      weak_method_factory_(this) {
  DCHECK(web_state_);

  translate_enabled_.Init(translate::prefs::kOfferTranslateEnabled, prefs);
  // Attempt to detect language since preloaded tabs will not execute
  // WebStateObserver::PageLoaded.
  StartLanguageDetection();
  web_state_->AddObserver(this);
  web::WebFramesManager* web_frames_manager =
      LanguageDetectionJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state);
  web_frames_manager->AddObserver(this);
}

IOSLanguageDetectionTabHelper::~IOSLanguageDetectionTabHelper() {
  for (auto& observer : observer_list_) {
    observer.IOSLanguageDetectionTabHelperWasDestroyed(this);
  }
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

void IOSLanguageDetectionTabHelper::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void IOSLanguageDetectionTabHelper::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void IOSLanguageDetectionTabHelper::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  // Update language histogram.
  if (url_language_histogram_ && details.is_model_reliable) {
    url_language_histogram_->OnPageVisited(details.model_detected_language);
  }

  for (auto& observer : observer_list_) {
    observer.OnLanguageDetermined(details);
  }
}

// web::WebFramesManager::Observer

void IOSLanguageDetectionTabHelper::WebFrameBecameAvailable(
    web::WebFramesManager* web_frames_manager,
    web::WebFrame* web_frame) {
  if (web_frame->IsMainFrame() && waiting_for_main_frame_) {
    waiting_for_main_frame_ = false;
    StartLanguageDetection();
  }
}

// web::WebStateObserver implementation:

void IOSLanguageDetectionTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  DCHECK_EQ(web_state_, web_state);
  if (load_completion_status == web::PageLoadCompletionStatus::SUCCESS)
    StartLanguageDetection();
}

void IOSLanguageDetectionTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  waiting_for_main_frame_ = false;
}

void IOSLanguageDetectionTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  DCHECK_EQ(web_state_, web_state);
  if (navigation_context->IsSameDocument()) {
    StartLanguageDetection();
  } else {
    ExtractContentLanguageHeader(navigation_context->GetResponseHeaders());
  }
}

void IOSLanguageDetectionTabHelper::WebStateDestroyed(
    web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

void IOSLanguageDetectionTabHelper::StartLanguageDetection() {
  // Translate setting should not cancel language detection, except if it is
  // disabled by policy.
  if (!translate_enabled_.GetValue() && translate_enabled_.IsManaged()) {
    return;
  }
  DCHECK(web_state_);
  const GURL& url = web_state_->GetVisibleURL();
  if (!web::UrlHasWebScheme(url) || !web_state_->ContentIsHTML())
    return;

  web::WebFramesManager* web_frames_manager =
      LanguageDetectionJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state_);
  web::WebFrame* web_frame = web_frames_manager->GetMainWebFrame();
  if (!web_frame) {
    waiting_for_main_frame_ = true;
    return;
  }

  LanguageDetectionJavaScriptFeature::GetInstance()->StartLanguageDetection(
      web_frame);
}

// Select the correct DeterminePageLanguage to call based on the feature flags.
std::string IOSLanguageDetectionTabHelper::DeterminePageLanguage(
    const std::string& code,
    const std::string& html_lang,
    const std::u16string& contents,
    std::string* model_detected_language,
    bool* is_model_reliable,
    float& model_reliability_score,
    std::string* detection_model_version) {
  if (translate::IsTFLiteLanguageDetectionEnabled() &&
      language_detection_model_ && language_detection_model_->IsAvailable()) {
    base::ElapsedTimer timer;
    std::string tflite_language =
        language_detection_model_->DeterminePageLanguage(
            code, html_lang, contents, model_detected_language,
            is_model_reliable, model_reliability_score);
    base::UmaHistogramTimes(
        kTranslateLanguageDetectionTFLiteModelEvaluationDuration,
        timer.Elapsed());

    *detection_model_version = language_detection_model_->GetModelVersion();
    base::UmaHistogramEnumeration(
        "IOS.Translate.PageLoad.LanguageDetectionMethod",
        LanguageDetectionMethod::kTFLiteModelUsed);
    return tflite_language;
  }

  if (translate::IsTFLiteLanguageDetectionEnabled()) {
    base::UmaHistogramEnumeration(
        "IOS.Translate.PageLoad.LanguageDetectionMethod",
        LanguageDetectionMethod::kTFLiteModelUnavailable);
  } else {
    base::UmaHistogramEnumeration(
        "IOS.Translate.PageLoad.LanguageDetectionMethod",
        LanguageDetectionMethod::kTFLiteModelDisabled);
  }
  return ::translate::DeterminePageLanguage(
      code, html_lang, contents, model_detected_language, is_model_reliable,
      model_reliability_score);
}

void IOSLanguageDetectionTabHelper::OnTextRetrieved(
    const bool has_notranslate,
    const std::string& js_http_content_language,
    const std::string& html_lang,
    const GURL& url,
    const base::Value* text_content) {
  if (!web_state_ || web_state_->IsBeingDestroyed()) {
    // If the webState is destroyed, this callback will still be called as the
    // request is cancelled.
    return;
  }
  // If there is no language defined in httpEquiv, use the HTTP header.
  const std::string http_content_language = js_http_content_language.empty()
                                                ? content_language_header_
                                                : js_http_content_language;

  std::string model_detected_language;
  bool is_model_reliable;
  float model_reliability_score = 0.0;
  std::u16string text = text_content && text_content->is_string()
                            ? base::UTF8ToUTF16(text_content->GetString())
                            : std::u16string();

  std::string detection_model_version = kCLDModelVersion;

  std::string language =
      DeterminePageLanguage(http_content_language, html_lang,
                            GetStringByClippingLastWord(text, kMaxIndexChars),
                            &model_detected_language, &is_model_reliable,
                            model_reliability_score, &detection_model_version);

  if (language.empty())
    return;  // No language detected.

  // Avoid an unnecessary copy of the full text content (which can be
  // ~64kB) until we need it on iOS (e.g. for the translate internals
  // page).
  translate::LanguageDetectionDetails details;
  details.time = base::Time::Now();
  details.url = url;
  details.has_notranslate = has_notranslate;
  details.content_language = http_content_language;
  details.model_detected_language = model_detected_language;
  details.is_model_reliable = is_model_reliable;
  details.html_root_language = html_lang;
  details.adopted_language = language;
  details.detection_model_version = detection_model_version;

  OnLanguageDetermined(details);
}

base::WeakPtr<IOSLanguageDetectionTabHelper>
IOSLanguageDetectionTabHelper::GetWeakPtr() {
  return weak_method_factory_.GetWeakPtr();
}

void IOSLanguageDetectionTabHelper::ExtractContentLanguageHeader(
    net::HttpResponseHeaders* headers) {
  if (!headers) {
    content_language_header_.clear();
    return;
  }

  headers->GetNormalizedHeader("content-language", &content_language_header_);
  // Remove everything after the comma ',' if any.
  size_t comma_index = content_language_header_.find_first_of(',');
  if (comma_index != std::string::npos)
    content_language_header_.resize(comma_index);
}

WEB_STATE_USER_DATA_KEY_IMPL(IOSLanguageDetectionTabHelper)

}  // namespace language
