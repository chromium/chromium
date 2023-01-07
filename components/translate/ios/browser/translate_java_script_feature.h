// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_IOS_BROWSER_TRANSLATE_JAVA_SCRIPT_FEATURE_H_
#define COMPONENTS_TRANSLATE_IOS_BROWSER_TRANSLATE_JAVA_SCRIPT_FEATURE_H_

#include "base/no_destructor.h"
#include "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class WebState;
}  // namespace web

namespace translate {

// Feature which listens for translate messages from the injected scripts.
class TranslateJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static TranslateJavaScriptFeature* GetInstance();

 private:
  friend class base::NoDestructor<TranslateJavaScriptFeature>;

  // web::JavaScriptFeature
  absl::optional<std::string> GetScriptMessageHandlerName() const override;
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
