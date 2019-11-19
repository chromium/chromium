// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/ios/browser/language_detection_controller.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/language/ios/browser/ios_language_detection_tab_helper.h"
#include "components/prefs/pref_member.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/translate/core/language_detection/language_detection_util.h"
#import "components/translate/ios/browser/js_language_detection_manager.h"
#include "components/translate/ios/browser/string_clipping_util.h"
#import "ios/web/common/url_scheme_util.h"
#include "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/navigation/navigation_context.h"
#include "net/http/http_response_headers.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace translate {

namespace {
// Name for the UMA metric used to track text extraction time.
const char kTranslateCaptureText[] = "Translate.CaptureText";
// Prefix for the language detection javascript commands. Must be kept in sync
// with language_detection.js.
const char kCommandPrefix[] = "languageDetection";
}

LanguageDetectionController::LanguageDetectionController(
    web::WebState* web_state,
    JsLanguageDetectionManager* manager,
    PrefService* prefs)
    : web_state_(web_state), js_manager_(manager), weak_method_factory_(this) {
  DCHECK(web_state_);
  DCHECK(js_manager_);

  translate_enabled_.Init(prefs::kOfferTranslateEnabled, prefs);
  web_state_->AddObserver(this);
  subscription_ = web_state_->AddScriptCommandCallback(
      base::Bind(&LanguageDetectionController::OnTextCaptured,
                 base::Unretained(this)),
      kCommandPrefix);
}

LanguageDetectionController::~LanguageDetectionController() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

void LanguageDetectionController::StartLanguageDetection() {
  if (!translate_enabled_.GetValue())
    return;  // Translate disabled in preferences.
  DCHECK(web_state_);
  const GURL& url = web_state_->GetVisibleURL();
  if (!web::UrlHasWebScheme(url) || !web_state_->ContentIsHTML())
    return;
  [js_manager_ inject];
  [js_manager_ startLanguageDetection];
}

void LanguageDetectionController::OnTextCaptured(
    const base::DictionaryValue& command,
    const GURL& url,
    bool user_is_interacting,
    web::WebFrame* sender_frame) {
  if (!sender_frame->IsMainFrame()) {
    // Translate is only supported on main frame.
    return;
  }
  std::string textCapturedCommand;
  if (!command.GetString("command", &textCapturedCommand) ||
      textCapturedCommand != "languageDetection.textCaptured" ||
      !command.HasKey("translationAllowed")) {
    NOTREACHED();
    return;
  }
  bool translation_allowed = false;
  command.GetBoolean("translationAllowed", &translation_allowed);
  if (!translation_allowed) {
    // Translation not allowed by the page. Done processing.
    return;
  }
  if (!command.HasKey("captureTextTime") || !command.HasKey("htmlLang") ||
      !command.HasKey("httpContentLanguage")) {
    NOTREACHED();
    return;
  }

  double capture_text_time = 0;
  command.GetDouble("captureTextTime", &capture_text_time);
  UMA_HISTOGRAM_TIMES(kTranslateCaptureText,
                      base::TimeDelta::FromMillisecondsD(capture_text_time));
  std::string html_lang;
  command.GetString("htmlLang", &html_lang);
  std::string http_content_language;
  command.GetString("httpContentLanguage", &http_content_language);
  // If there is no language defined in httpEquiv, use the HTTP header.
  if (http_content_language.empty())
    http_content_language = content_language_header_;

  [js_manager_ retrieveBufferedTextContent:
                   base::Bind(&LanguageDetectionController::OnTextRetrieved,
                              weak_method_factory_.GetWeakPtr(),
                              http_content_language, html_lang, url)];
}

void LanguageDetectionController::OnTextRetrieved(
    const std::string& http_content_language,
    const std::string& html_lang,
    const GURL& url,
    const base::string16& text_content) {
  std::string cld_language;
  bool is_cld_reliable;
  std::string language = translate::DeterminePageLanguage(
      http_content_language, html_lang,
      GetStringByClippingLastWord(text_content,
                                  language_detection::kMaxIndexChars),
      &cld_language, &is_cld_reliable);
  if (language.empty())
    return;  // No language detected.

  // Avoid an unnecessary copy of the full text content (which can be
  // ~64kB) until we need it on iOS (e.g. for the translate internals
  // page).
  LanguageDetectionDetails details;
  details.time = base::Time::Now();
  details.url = url;
  details.content_language = http_content_language;
  details.cld_language = cld_language;
  details.is_cld_reliable = is_cld_reliable;
  details.html_root_language = html_lang;
  details.adopted_language = language;

  language::IOSLanguageDetectionTabHelper::FromWebState(web_state_)
      ->OnLanguageDetermined(details);
}

void LanguageDetectionController::ExtractContentLanguageHeader(
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

// web::WebStateObserver implementation:

void LanguageDetectionController::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  DCHECK_EQ(web_state_, web_state);
  if (load_completion_status == web::PageLoadCompletionStatus::SUCCESS)
    StartLanguageDetection();
}

void LanguageDetectionController::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  DCHECK_EQ(web_state_, web_state);
  if (navigation_context->IsSameDocument()) {
    StartLanguageDetection();
  } else {
    ExtractContentLanguageHeader(navigation_context->GetResponseHeaders());
  }
}

void LanguageDetectionController::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

}  // namespace translate
