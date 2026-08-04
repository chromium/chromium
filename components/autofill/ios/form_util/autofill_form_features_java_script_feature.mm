// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/form_util/autofill_form_features_java_script_feature.h"

#import "base/functional/bind.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/common/features.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"

namespace {
const char kFeaturesScriptName[] = "autofill_form_features";

// Helper function to return the dynamic map of feature placeholders to their
// corresponding C++ feature states.
web::JavaScriptFeature::FeatureScript::PlaceholderReplacements
GetReplacements() {
  return @{
    @"gCrWebPlaceholderAutofillAcrossIframesEnabled" :
            base::FeatureList::IsEnabled(
                autofill::features::kAutofillAcrossIframesIos)
        ? @"true"
        : @"false",
    @"gCrWebPlaceholderAutofillAcrossIframesThrottling" :
            base::FeatureList::IsEnabled(
                autofill::features::kAutofillAcrossIframesIosThrottling)
        ? @"true"
        : @"false",
    @"gCrWebPlaceholderAutofillDisallowMoreHyphenLikeLabels" :
            base::FeatureList::IsEnabled(
                autofill::features::kAutofillDisallowMoreHyphenLikeLabels)
        ? @"true"
        : @"false",
    @"gCrWebPlaceholderAutofillIgnoreCheckableElements" :
            base::FeatureList::IsEnabled(
                autofill::features::kAutofillIgnoreCheckableElements)
        ? @"true"
        : @"false",
    @"gCrWebPlaceholderAutofillSupportDateInput" :
            base::FeatureList::IsEnabled(::kAutofillSupportDateInput)
        ? @"true"
        : @"false",
    @"gCrWebPlaceholderAutofillCorrectUserEditedBitInParsedField" :
            base::FeatureList::IsEnabled(
                ::kAutofillCorrectUserEditedBitInParsedField)
        ? @"true"
        : @"false",
    @"gCrWebPlaceholderAutofillAllowDefaultPreventedSubmission" :
            base::FeatureList::IsEnabled(
                ::kAutofillAllowDefaultPreventedSubmission)
        ? @"true"
        : @"false",
    @"gCrWebPlaceholderAutofillDedupeFormSubmission" :
            base::FeatureList::IsEnabled(::kAutofillDedupeFormSubmission)
        ? @"true"
        : @"false",
    @"gCrWebPlaceholderAutofillEmailVerification" :
            base::FeatureList::IsEnabled(::kAutofillEmailVerification)
        ? @"true"
        : @"false",
    @"gCrWebPlaceholderAutofillReportFormSubmissionErrors" :
            base::FeatureList::IsEnabled(::kAutofillReportFormSubmissionErrors)
        ? @"true"
        : @"false",
    @"gCrWebPlaceholderAutofillCountFormSubmissionInRenderer" :
            base::FeatureList::IsEnabled(
                ::kAutofillCountFormSubmissionInRenderer)
        ? @"true"
        : @"false",
    @"gCrWebPlaceholderAutofillTrackPasswordFieldsIos" :
            base::FeatureList::IsEnabled(::kAutofillTrackPasswordFieldsIos)
        ? @"true"
        : @"false",
  };
}
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
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow,
              base::BindRepeating(&GetReplacements))},
          {
              web::java_script_features::GetBaseJavaScriptFeature(),
          }) {}

AutofillFormFeaturesJavaScriptFeature::
    ~AutofillFormFeaturesJavaScriptFeature() = default;

}  // namespace autofill
