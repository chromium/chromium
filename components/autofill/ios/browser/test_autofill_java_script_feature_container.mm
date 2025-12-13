// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/test_autofill_java_script_feature_container.h"

#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/form_util/form_handlers_java_script_feature.h"
#import "components/autofill/ios/form_util/remote_frame_registration_java_script_feature.h"

namespace autofill {

TestAutofillJavaScriptFeatureContainer::
    TestAutofillJavaScriptFeatureContainer() = default;

TestAutofillJavaScriptFeatureContainer::
    ~TestAutofillJavaScriptFeatureContainer() {
  delete form_handlers_java_script_feature_;
  delete autofill_java_script_feature_;
  delete remote_frame_registration_java_script_feature_;
}

FormHandlersJavaScriptFeature*
TestAutofillJavaScriptFeatureContainer::form_handlers_java_script_feature() {
  if (!form_handlers_java_script_feature_) {
    // Create the form handlers feature using the self contained
    // FormUtilJavaScriptFeature instance. This way the form util instance is
    // created for the correct content world.
    form_handlers_java_script_feature_ = new FormHandlersJavaScriptFeature(
        remote_frame_registration_java_script_feature());
  }

  return form_handlers_java_script_feature_;
}

AutofillJavaScriptFeature*
TestAutofillJavaScriptFeatureContainer::autofill_java_script_feature() {
  if (!autofill_java_script_feature_) {
    autofill_java_script_feature_ = new AutofillJavaScriptFeature();
  }
  return autofill_java_script_feature_;
}

RemoteFrameRegistrationJavaScriptFeature*
TestAutofillJavaScriptFeatureContainer::
    remote_frame_registration_java_script_feature() {
  if (!remote_frame_registration_java_script_feature_) {
    remote_frame_registration_java_script_feature_ =
        new RemoteFrameRegistrationJavaScriptFeature();
  }
  return remote_frame_registration_java_script_feature_;
}

}  // namespace autofill
