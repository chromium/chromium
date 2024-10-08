// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_FORM_UTIL_AUTOFILL_FORM_FEATURES_JAVA_SCRIPT_FEATURE_H_
#define COMPONENTS_AUTOFILL_IOS_FORM_UTIL_AUTOFILL_FORM_FEATURES_JAVA_SCRIPT_FEATURE_H_

#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class WebFrame;
}  // namespace web

namespace autofill {

// Communicates with the JavaScript file, autofill_form_features.js, which
// contains functions for setting form features flags in the renderer.
class AutofillFormFeaturesJavaScriptFeature : public web::JavaScriptFeature {
 public:
  // This feature holds no state, so only a single static instance is ever
  // needed.
  static AutofillFormFeaturesJavaScriptFeature* GetInstance();

  // Enables/disables the AutofillAcrossIframes feature in `frame`.
  void SetAutofillAcrossIframes(web::WebFrame* frame, bool enabled);

  // Enables/disables the renderer side behaviours in `frame` needed for
  // Autofill features to work in an isolated content world.
  void SetAutofillIsolatedContentWorld(web::WebFrame* frame, bool enabled);

 private:
  friend class base::NoDestructor<AutofillFormFeaturesJavaScriptFeature>;
  // Friend test feature container so it can create instances of this class.
  // This JS feature is injected in different content worlds depending on a
  // feature flag. Tests need to create new instances of the JS feature when the
  // feature flag changes.
  // TODO(crbug.com/359538514): Remove friend once isolated world for Autofill
  // is launched.
  friend class TestAutofillJavaScriptFeatureContainer;

  AutofillFormFeaturesJavaScriptFeature();
  ~AutofillFormFeaturesJavaScriptFeature() override;

  AutofillFormFeaturesJavaScriptFeature(
      const AutofillFormFeaturesJavaScriptFeature&) = delete;
  AutofillFormFeaturesJavaScriptFeature& operator=(
      const AutofillFormFeaturesJavaScriptFeature&) = delete;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_FORM_UTIL_AUTOFILL_FORM_FEATURES_JAVA_SCRIPT_FEATURE_H_
