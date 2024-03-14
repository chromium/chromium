// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/language/ios/browser/language_detection_java_script_feature.h"

#import "base/metrics/histogram_macros.h"
#import "components/language/ios/browser/ios_language_detection_tab_helper.h"
#import "components/prefs/pref_member.h"
#import "components/translate/core/browser/translate_pref_names.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"

namespace {

const char kScriptName[] = "language_detection";
const char kLanguageDetectionTextCapturedMessageHandlerName[] =
    "LanguageDetectionTextCaptured";

}  // namespace

namespace language {

// static
LanguageDetectionJavaScriptFeature*
LanguageDetectionJavaScriptFeature::GetInstance() {
  static base::NoDestructor<LanguageDetectionJavaScriptFeature> instance;
  return instance.get();
}

LanguageDetectionJavaScriptFeature::LanguageDetectionJavaScriptFeature()
    : web::JavaScriptFeature(
          web::ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kMainFrame,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)}) {}

LanguageDetectionJavaScriptFeature::~LanguageDetectionJavaScriptFeature() =
    default;

void LanguageDetectionJavaScriptFeature::StartLanguageDetection(
    web::WebFrame* frame) {
  CallJavaScriptFunction(frame, "languageDetection.detectLanguage", {});
}

void LanguageDetectionJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& script_message) {
  if (!script_message.is_main_frame()) {
    // Translate is only supported on main frame.
    return;
  }

  if (!script_message.body() || !script_message.body()->is_dict()) {
    return;
  }

  base::Value::Dict& body_dict = script_message.body()->GetDict();

  std::optional<bool> has_notranslate = body_dict.FindBool("hasNoTranslate");
  const std::string* html_lang = body_dict.FindString("htmlLang");
  const std::string* http_content_language =
      body_dict.FindString("httpContentLanguage");
  const std::string* frame_id = body_dict.FindString("frameId");
  if (!has_notranslate.has_value() || !html_lang || !http_content_language ||
      !frame_id) {
    return;
  }

  web::WebFrame* sender_frame =
      GetWebFramesManager(web_state)->GetFrameWithId(*frame_id);
  if (!sender_frame) {
    return;
  }

  if (!script_message.request_url()) {
    return;
  }

  GURL url = script_message.request_url().value();

  language::IOSLanguageDetectionTabHelper* tab_helper =
      language::IOSLanguageDetectionTabHelper::FromWebState(web_state);
  if (!tab_helper) {
    return;
  }

  CallJavaScriptFunction(
      sender_frame, "languageDetection.retrieveBufferedTextContent", {},
      base::BindOnce(&IOSLanguageDetectionTabHelper::OnTextRetrieved,
                          tab_helper->GetWeakPtr(), *has_notranslate,
                          *http_content_language, *html_lang, url),
      base::Milliseconds(web::kJavaScriptFunctionCallDefaultTimeout));
}

std::optional<std::string>
LanguageDetectionJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kLanguageDetectionTextCapturedMessageHandlerName;
}

}  // namespace language
