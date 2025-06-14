// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/form_util/autofill_form_features_java_script_feature.h"

#import "ios/web/public/js_messaging/java_script_feature_util.h"

namespace {
const char kFeaturesScriptName[] = "autofill_form_features";
}  // namespace

namespace autofill {

// static
AutofillFormFeaturesJavaScriptFeature*
AutofillFormFeaturesJavaScriptFeature::GetInstance() {
  static base::NoDestructor<AutofillFormFeaturesJavaScriptFeature> instance;
  return instance.get();
}

AutofillFormFeaturesJavaScriptFeature::AutofillFormFeaturesJavaScriptFeature()
    : web::JavaScriptFeature(
          web::ContentWorld::kAllContentWorlds,
          {FeatureScript::CreateWithFilename(
              kFeaturesScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)},
          {
              web::java_script_features::GetBaseJavaScriptFeature(),
          }) {}

AutofillFormFeaturesJavaScriptFeature::
    ~AutofillFormFeaturesJavaScriptFeature() = default;

void AutofillFormFeaturesJavaScriptFeature::SetAutofillAcrossIframes(
    web::WebFrame* frame,
    bool enabled) {
  CHECK(frame);
  frame->CallJavaScriptFunction(
      "autofill_form_features.setAutofillAcrossIframes",
      base::Value::List().Append(enabled));
}

void AutofillFormFeaturesJavaScriptFeature::SetAutofillDisallowSlashDotLabels(
    web::WebFrame* frame,
    bool enabled) {
  CHECK(frame);
  frame->CallJavaScriptFunction(
      "autofill_form_features.setAutofillDisallowSlashDotLabels",
      base::Value::List().Append(enabled));
}

void AutofillFormFeaturesJavaScriptFeature::SetAutofillAcrossIframesThrottling(
    web::WebFrame* frame,
    bool enabled) {
  CHECK(frame);
  frame->CallJavaScriptFunction(
      "autofill_form_features.setAutofillAcrossIframesThrottling",
      base::Value::List().Append(enabled));
}

void AutofillFormFeaturesJavaScriptFeature::SetAutofillIsolatedContentWorld(
    web::WebFrame* frame,
    bool enabled) {
  CHECK(frame);
  frame->CallJavaScriptFunction(
      "autofill_form_features.setAutofillIsolatedContentWorld",
      base::Value::List().Append(enabled));
}

void AutofillFormFeaturesJavaScriptFeature::
    SetAutofillCorrectUserEditedBitInParsedField(web::WebFrame* frame,
                                                 bool enabled) {
  CHECK(frame);
  frame->CallJavaScriptFunction(
      "autofill_form_features.setAutofillCorrectUserEditedBitInParsedField",
      base::Value::List().Append(enabled));
}

void AutofillFormFeaturesJavaScriptFeature::
    SetAutofillAllowDefaultPreventedFormSubmission(web::WebFrame* frame,
                                                   bool enabled) {
  CHECK(frame);
  frame->CallJavaScriptFunction(
      "autofill_form_features.setAutofillAllowDefaultPreventedSubmission",
      base::Value::List().Append(enabled));
}

void AutofillFormFeaturesJavaScriptFeature::SetAutofillDedupeFormSubmission(
    web::WebFrame* frame,
    bool enabled) {
  CHECK(frame);
  frame->CallJavaScriptFunction(
      "autofill_form_features.setAutofillDedupeFormSubmission",
      base::Value::List().Append(enabled));
}

// Enables/disables reporting form submission errors.
void AutofillFormFeaturesJavaScriptFeature::
    SetAutofillReportFormSubmissionErrors(web::WebFrame* frame, bool enabled) {
  frame->CallJavaScriptFunction(
      "autofill_form_features.setAutofillReportFormSubmissionErrors",
      base::Value::List().Append(enabled));
}

}  // namespace autofill
