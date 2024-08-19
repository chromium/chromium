// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_FORM_UTIL_FORM_UTIL_JAVA_SCRIPT_FEATURE_H_
#define COMPONENTS_AUTOFILL_IOS_FORM_UTIL_FORM_UTIL_JAVA_SCRIPT_FEATURE_H_

#include "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class WebFrame;
}  // namespace web

namespace autofill {

// Communicates with the JavaScript file, fill.js, which contains form util
// functions.
class FormUtilJavaScriptFeature : public web::JavaScriptFeature {
 public:
  // This feature holds no state, so only a single static instance is ever
  // needed.
  static FormUtilJavaScriptFeature* GetInstance();

  // Enables/disables the AutofillAcrossIframes feature in `frame`.
  void SetAutofillAcrossIframes(web::WebFrame* frame, bool enabled);

  // Enables/disables XHR form submission detection in `frame`.
  void SetAutofillXHRSubmissionDetection(web::WebFrame* frame, bool enabled);

  // Enables/disables the renderer side behaviours in `frame` needed for
  // Autofill features to work in an isolated content world.
  void SetAutofillIsolatedContentWorld(web::WebFrame* frame, bool enabled);

 private:
  friend class base::NoDestructor<FormUtilJavaScriptFeature>;

  FormUtilJavaScriptFeature();
  ~FormUtilJavaScriptFeature() override;

  FormUtilJavaScriptFeature(const FormUtilJavaScriptFeature&) = delete;
  FormUtilJavaScriptFeature& operator=(const FormUtilJavaScriptFeature&) =
      delete;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_FORM_UTIL_FORM_UTIL_JAVA_SCRIPT_FEATURE_H_
