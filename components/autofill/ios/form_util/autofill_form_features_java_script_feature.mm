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
      base::ListValue().Append(enabled));
}

void AutofillFormFeaturesJavaScriptFeature::
    SetAutofillDisallowMoreHyphenLikeLabels(web::WebFrame* frame,
                                            bool enabled) {
  CHECK(frame);
  frame->CallJavaScriptFunction(
      "autofill_form_features.setAutofillDisallowMoreHyphenLikeLabels",
      base::ListValue().Append(enabled));
}

void AutofillFormFeaturesJavaScriptFeature::SetAutofillDisallowSlashDotLabels(
    web::WebFrame* frame,
    bool enabled) {
  CHECK(frame);
  frame->CallJavaScriptFunction(
      "autofill_form_features.setAutofillDisallowSlashDotLabels",
      base::ListValue().Append(enabled));
}

void AutofillFormFeaturesJavaScriptFeature::SetAutofillAcrossIframesThrottling(
    web::WebFrame* frame,
    bool enabled) {
  CHECK(frame);
  frame->CallJavaScriptFunction(
      "autofill_form_features.setAutofillAcrossIframesThrottling",
      base::ListValue().Append(enabled));
}

void AutofillFormFeaturesJavaScriptFeature::SetAutofillIgnoreCheckableElements(
    web::WebFrame* frame,
    bool enabled) {
  CHECK(frame);
  frame->CallJavaScriptFunction(
      "autofill_form_features.setAutofillIgnoreCheckableElements",
      base::ListValue().Append(enabled));
}

void AutofillFormFeaturesJavaScriptFeature::SetAutofillIsolatedContentWorld(
    web::WebFrame* frame,
    bool enabled) {
  CHECK(frame);
  frame->CallJavaScriptFunction(
      "autofill_form_features.setAutofillIsolatedContentWorld",
      base::ListValue().Append(enabled));
}

void AutofillFormFeaturesJavaScriptFeature::
    SetAutofillCorrectUserEditedBitInParsedField(web::WebFrame* frame,
                                                 bool enabled) {
  CHECK(frame);
  frame->CallJavaScriptFunction(
      "autofill_form_features.setAutofillCorrectUserEditedBitInParsedField",
      base::ListValue().Append(enabled));
}

void AutofillFormFeaturesJavaScriptFeature::
    SetAutofillAllowDefaultPreventedFormSubmission(web::WebFrame* frame,
                                                   bool enabled) {
  CHECK(frame);
  frame->CallJavaScriptFunction(
      "autofill_form_features.setAutofillAllowDefaultPreventedSubmission",
      base::ListValue().Append(enabled));
}

void AutofillFormFeaturesJavaScriptFeature::SetAutofillDedupeFormSubmission(
    web::WebFrame* frame,
    bool enabled) {
  CHECK(frame);
  frame->CallJavaScriptFunction(
      "autofill_form_features.setAutofillDedupeFormSubmission",
      base::ListValue().Append(enabled));
}

// Enables/disables reporting form submission errors.
void AutofillFormFeaturesJavaScriptFeature::
    SetAutofillReportFormSubmissionErrors(web::WebFrame* frame, bool enabled) {
  frame->CallJavaScriptFunction(
      "autofill_form_features.setAutofillReportFormSubmissionErrors",
      base::ListValue().Append(enabled));
}

void AutofillFormFeaturesJavaScriptFeature::
    SetAutofillCountFormSubmissionInRenderer(web::WebFrame* frame,
                                             bool enabled) {
  frame->CallJavaScriptFunction(
      "autofill_form_features.setAutofillCountFormSubmissionInRenderer",
      base::ListValue().Append(enabled));
}

}  // namespace autofill
