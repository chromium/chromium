// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/test_autofill_java_script_feature_container.h"

#import "components/autofill/ios/form_util/autofill_form_features_java_script_feature.h"
#import "components/autofill/ios/form_util/form_handlers_java_script_feature.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"

namespace autofill {

TestAutofillJavaScriptFeatureContainer::
    TestAutofillJavaScriptFeatureContainer() = default;

TestAutofillJavaScriptFeatureContainer::
    ~TestAutofillJavaScriptFeatureContainer() {
  delete form_util_java_script_feature_;
  delete form_handlers_java_script_feature_;
}

FormUtilJavaScriptFeature*
TestAutofillJavaScriptFeatureContainer::form_util_java_script_feature() {
  if (!form_util_java_script_feature_) {
    form_util_java_script_feature_ = new FormUtilJavaScriptFeature();
  }

  return form_util_java_script_feature_;
}

FormHandlersJavaScriptFeature*
TestAutofillJavaScriptFeatureContainer::form_handlers_java_script_feature() {
  if (!form_handlers_java_script_feature_) {
    // Create the form handlers feature using the self contained
    // FormUtilJavaScriptFeature instance. This way the form util instance is
    // created for the correct content world.
    form_handlers_java_script_feature_ =
        new FormHandlersJavaScriptFeature(form_util_java_script_feature());
  }

  return form_handlers_java_script_feature_;
}

AutofillFormFeaturesJavaScriptFeature* TestAutofillJavaScriptFeatureContainer::
    autofill_form_features_java_script_feature() {
  if (!autofill_form_features_java_script_feature_) {
    autofill_form_features_java_script_feature_ =
        new AutofillFormFeaturesJavaScriptFeature();
  }

  return autofill_form_features_java_script_feature_;
}

}  // namespace autofill
