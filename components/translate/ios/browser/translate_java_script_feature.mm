// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/translate/ios/browser/translate_java_script_feature.h"

#import "components/translate/ios/browser/translate_controller.h"
#import "ios/web/public/js_messaging/script_message.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
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
          web::ContentWorld::kPageContentWorld,
          {/* The `translate_ios` script is injected on demand */
           /* by JSTranslateWebFrameManager. */}) {}

TranslateJavaScriptFeature::~TranslateJavaScriptFeature() = default;

absl::optional<std::string>
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
