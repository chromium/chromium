// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_TEST_AUTOFILL_JAVA_SCRIPT_FEATURE_CONTAINER_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_TEST_AUTOFILL_JAVA_SCRIPT_FEATURE_CONTAINER_H_

#import "base/memory/raw_ptr.h"

namespace autofill {

class FormHandlersJavaScriptFeature;
class AutofillFormFeaturesJavaScriptFeature;
class AutofillRendererIDJavaScriptFeature;

/*
 Holds instances of JavaScriptFeature classes related to Autofill. Use this
 object instead of the statically allocated instances of the features when
 testing injecting Autofill in different javascript content worlds. Using the
 statically stored instances of the features is not an option because once the
 feature instances are created, they can't be mutated. This means the features
 would be injected in the same content world, regardless of the value of the
 feature flag controlling the content world for Autofill. Tests can use this
 container to create new instances of javascript features that are injected in
 the content world dictated by the feature flag.

 The features instances are lazily evaluated and remain in memory until this
 object is destroyed.

 TODO(crbug.com/359538514): Remove this class once Autofill is moved to the
 isolated content world.
 */
class TestAutofillJavaScriptFeatureContainer {
 public:
  TestAutofillJavaScriptFeatureContainer();
  ~TestAutofillJavaScriptFeatureContainer();

  FormHandlersJavaScriptFeature* form_handlers_java_script_feature();
  AutofillFormFeaturesJavaScriptFeature*
  autofill_form_features_java_script_feature();

  AutofillRendererIDJavaScriptFeature*
  autofill_renderer_id_java_script_feature();

 private:
  TestAutofillJavaScriptFeatureContainer(
      const TestAutofillJavaScriptFeatureContainer&) = delete;
  TestAutofillJavaScriptFeatureContainer& operator=(
      const TestAutofillJavaScriptFeatureContainer&) = delete;

  raw_ptr<FormHandlersJavaScriptFeature> form_handlers_java_script_feature_ =
      nullptr;
  raw_ptr<AutofillFormFeaturesJavaScriptFeature>
      autofill_form_features_java_script_feature_ = nullptr;
  raw_ptr<AutofillRendererIDJavaScriptFeature>
      autofill_renderer_id_java_script_feature_ = nullptr;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_TEST_AUTOFILL_JAVA_SCRIPT_FEATURE_CONTAINER_H_
