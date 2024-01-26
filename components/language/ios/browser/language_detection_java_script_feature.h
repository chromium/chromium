// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_IOS_BROWSER_LANGUAGE_DETECTION_JAVA_SCRIPT_FEATURE_H_
#define COMPONENTS_LANGUAGE_IOS_BROWSER_LANGUAGE_DETECTION_JAVA_SCRIPT_FEATURE_H_

#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace language {

class LanguageDetectionJavaScriptFeature : public web::JavaScriptFeature {
 public:
  // Returns the shared LanguageDetectionJavaScriptFeature instance.
  static LanguageDetectionJavaScriptFeature* GetInstance();

  // Triggers the start of JS language detection within `frame`.
  void StartLanguageDetection(web::WebFrame* frame);

 private:
  friend class base::NoDestructor<LanguageDetectionJavaScriptFeature>;

  LanguageDetectionJavaScriptFeature();
  ~LanguageDetectionJavaScriptFeature() override;

  LanguageDetectionJavaScriptFeature(
      const LanguageDetectionJavaScriptFeature&) = delete;
  LanguageDetectionJavaScriptFeature& operator=(
      const LanguageDetectionJavaScriptFeature&) = delete;

  // JavaScriptFeature:
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& script_message) override;
  std::optional<std::string> GetScriptMessageHandlerName() const override;
};

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_IOS_BROWSER_LANGUAGE_DETECTION_JAVA_SCRIPT_FEATURE_H_
