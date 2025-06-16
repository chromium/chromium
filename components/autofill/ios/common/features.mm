// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/common/features.h"

// Keep the alphabetical order.

BASE_FEATURE(kAddAddressManually,
             "AddAddressManually",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAddAddressManuallyEnabled() {
  return base::FeatureList::IsEnabled(kAddAddressManually) &&
         base::FeatureList::IsEnabled(
             kAutofillDynamicallyLoadsFieldsForAddressInput);
}

// LINT.IfChange(autofill_allow_default_prevented_submission)
BASE_FEATURE(kAutofillAllowDefaultPreventedSubmission,
             "AutofillAllowDefaultPreventedSubmission",
             base::FEATURE_DISABLED_BY_DEFAULT);
// LINT.ThenChange(/components/autofill/ios/form_util/resources/autofill_form_features.ts:autofill_allow_default_prevented_submission)

// LINT.IfChange(autofill_correct_user_edited_bit_in_parsed_field)
BASE_FEATURE(kAutofillCorrectUserEditedBitInParsedField,
             "AutofillCorrectUserEditedBitInParsedField",
             base::FEATURE_ENABLED_BY_DEFAULT);
// LINT.ThenChange(/components/autofill/ios/form_util/resources/autofill_form_features.ts:autofill_correct_user_edited_bit_in_parsed_field)

// LINT.IfChange(autofill_dedupe_form_submission)
BASE_FEATURE(kAutofillDedupeFormSubmission,
             "AutofillDedupeFormSubmission",
             base::FEATURE_DISABLED_BY_DEFAULT);
// LINT.ThenChange(/components/autofill/ios/form_util/resources/autofill_form_features.ts:autofill_dedupe_form_submission)

BASE_FEATURE(kAutofillDynamicallyLoadsFieldsForAddressInput,
             "AutofillDynamicallyLoadsFieldsForAddressInput",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAutofillFixXhrForXframe,
             "AutofillFixXhrForXframe",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAutofillFormSubmissionEventsInCaptureMode,
             "AutofillFormSubmissionEventsInCaptureMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

// LINT.IfChange(autofill_isolated_content_world)
BASE_FEATURE(kAutofillIsolatedWorldForJavascriptIos,
             "AutofillIsolatedWorldForJavascriptIos",
             base::FEATURE_DISABLED_BY_DEFAULT);
// LINT.ThenChange(/components/autofill/ios/form_util/resources/autofill_form_features.ts:autofill_isolated_content_world)

BASE_FEATURE(kAutofillPaymentsSheetV2Ios,
             "AutofillPaymentsSheetV2Ios",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAutofillPaymentsSheetV3Ios,
             "AutofillPaymentsSheetV3Ios",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAutofillRefillForFormsIos,
             "AutofillRefillForFormsIos",
             base::FEATURE_DISABLED_BY_DEFAULT);

// LINT.IfChange(autofill_report_form_submission_errors)
BASE_FEATURE(kAutofillReportFormSubmissionErrors,
             "AutofillReportFormSubmissionErrors",
             base::FEATURE_DISABLED_BY_DEFAULT);
// LINT.ThenChange(/components/autofill/ios/form_util/resources/autofill_form_features.ts:autofill_report_form_submission_errors)

BASE_FEATURE(kAutofillStickyInfobarIos,
             "AutofillStickyInfobarIos",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAutofillThrottleDocumentFormScanIos,
             "AutofillThrottleDocumentFormScanIos",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Minimal period of time between the document form scanning batches.
extern const base::FeatureParam<int> kAutofillDocumentFormScanPeriodMs = {
    &kAutofillThrottleDocumentFormScanIos,
    /*name=*/"period-ms", /*default_value=*/250};

BASE_FEATURE(kAutofillThrottleDocumentFormScanForceFirstScanIos,
             "AutofillThrottleDocumentFormScanForceFirstScanIos",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAutofillThrottleFilteredDocumentFormScanIos,
             "AutofillThrottleFilteredDocumentFormScanIos",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Minimal period of time between the filtered document form scanning batches.
extern const base::FeatureParam<int> kAutofillFilteredDocumentFormScanPeriodMs =
    {&kAutofillThrottleFilteredDocumentFormScanIos,
     /*name=*/"period-ms", /*default_value=*/250};
