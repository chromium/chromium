// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/translate/ios/browser/translate_java_script_feature.h"

#import "base/check_op.h"
#import "base/strings/utf_string_conversions.h"
#import "components/translate/ios/browser/translate_controller.h"
#import "ios/web/public/js_messaging/script_message.h"

namespace {
constexpr char kScriptName[] = "translate_ios";
constexpr char kScriptMessageName[] = "TranslateMessage";
}  // namespace

namespace translate {

// static
TranslateJavaScriptFeature* TranslateJavaScriptFeature::GetInstance() {
  static base::NoDestructor<TranslateJavaScriptFeature> instance;
  return instance.get();
}

TranslateJavaScriptFeature::TranslateJavaScriptFeature()
    : web::JavaScriptFeature(
          web::ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kMainFrame,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)}) {}

TranslateJavaScriptFeature::~TranslateJavaScriptFeature() = default;

void TranslateJavaScriptFeature::StartTranslation(
    web::WebFrame* frame,
    const std::string& source_language,
    const std::string& target_language) {
  if (!frame) {
    return;
  }

  base::Value::List parameters;
  parameters.Append(source_language);
  parameters.Append(target_language);
  CallJavaScriptFunction(frame, "translate.startTranslation", parameters);
}

void TranslateJavaScriptFeature::RevertTranslation(web::WebFrame* frame) {
  if (!frame) {
    return;
  }

  CallJavaScriptFunction(frame, "translate.revertTranslation", {});
}

void TranslateJavaScriptFeature::InjectTranslateScript(
    web::WebFrame* frame,
    const std::string script) {
  if (!frame) {
    return;
  }

  // Always revert translation since this page could have been loaded from the
  // WebKit page cache.
  RevertTranslation(frame);

  ExecuteJavaScript(frame, base::UTF8ToUTF16(script),
                    base::DoNothingAs<void(const base::Value*, NSError*)>());
}

std::optional<std::string>
TranslateJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kScriptMessageName;
}

void TranslateJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  if (!message.is_main_frame() || !message.body() ||
      !message.body()->is_dict()) {
    return;
  }

  TranslateController* translate_controller =
      TranslateController::FromWebState(web_state);
  translate_controller->OnJavascriptCommandReceived(message.body()->GetDict());
}

}  // namespace translate
