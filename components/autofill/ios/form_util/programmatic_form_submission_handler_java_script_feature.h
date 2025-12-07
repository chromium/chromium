// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_FORM_UTIL_PROGRAMMATIC_FORM_SUBMISSION_HANDLER_JAVA_SCRIPT_FEATURE_H_
#define COMPONENTS_AUTOFILL_IOS_FORM_UTIL_PROGRAMMATIC_FORM_SUBMISSION_HANDLER_JAVA_SCRIPT_FEATURE_H_

#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class WebState;
}  // namespace web

namespace autofill {

// Installs a listener that gets called whenever HTMLElement.submit() is
// invoked.
class ProgrammaticFormSubmissionHandlerJavaScriptFeature
    : public web::JavaScriptFeature {
 public:
  // This feature holds no state, so only a single static instance is ever
  // needed.
  static ProgrammaticFormSubmissionHandlerJavaScriptFeature* GetInstance();

 private:
  friend class base::NoDestructor<
      ProgrammaticFormSubmissionHandlerJavaScriptFeature>;

  // web::JavaScriptFeature
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;

  ProgrammaticFormSubmissionHandlerJavaScriptFeature();
  ~ProgrammaticFormSubmissionHandlerJavaScriptFeature() override;

  ProgrammaticFormSubmissionHandlerJavaScriptFeature(
      const ProgrammaticFormSubmissionHandlerJavaScriptFeature&) = delete;
  ProgrammaticFormSubmissionHandlerJavaScriptFeature& operator=(
      const ProgrammaticFormSubmissionHandlerJavaScriptFeature&) = delete;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_FORM_UTIL_PROGRAMMATIC_FORM_SUBMISSION_HANDLER_JAVA_SCRIPT_FEATURE_H_
