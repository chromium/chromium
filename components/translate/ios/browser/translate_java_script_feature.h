// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_IOS_BROWSER_TRANSLATE_JAVA_SCRIPT_FEATURE_H_
#define COMPONENTS_TRANSLATE_IOS_BROWSER_TRANSLATE_JAVA_SCRIPT_FEATURE_H_

#include "base/no_destructor.h"
#include "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class WebFrame;
class WebState;
}  // namespace web

namespace translate {

// Feature which listens for translate messages from the injected scripts.
class TranslateJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static TranslateJavaScriptFeature* GetInstance();

  // Starts translation from `source_language` to `target_language`.
  // No ops if `frame` is null.
  void StartTranslation(web::WebFrame* frame,
                        const std::string& source_language,
                        const std::string& target_language);

  // Reverts any translation that was previously performed.
  // No ops if `frame` is null.
  void RevertTranslation(web::WebFrame* frame);

  // Injects the translate element script fetched from translate_script.cc.
  // This function should not be used for any other purpose.
  // No ops if `frame` is null.
  void InjectTranslateScript(web::WebFrame* frame, const std::string script);

 private:
  friend class base::NoDestructor<TranslateJavaScriptFeature>;

  // web::JavaScriptFeature
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;

  TranslateJavaScriptFeature();
  ~TranslateJavaScriptFeature() override;

  TranslateJavaScriptFeature(const TranslateJavaScriptFeature&) = delete;
  TranslateJavaScriptFeature& operator=(const TranslateJavaScriptFeature&) =
      delete;
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_IOS_BROWSER_TRANSLATE_JAVA_SCRIPT_FEATURE_H_
