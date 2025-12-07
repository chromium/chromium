// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/common/features.h"

// Keep the alphabetical order.

// LINT.IfChange(autofill_allow_default_prevented_submission)
BASE_FEATURE(kAutofillAllowDefaultPreventedSubmission,
             base::FEATURE_ENABLED_BY_DEFAULT);
// LINT.ThenChange(/components/autofill/ios/form_util/resources/autofill_form_features.ts:autofill_allow_default_prevented_submission)

// LINT.IfChange(autofill_correct_user_edited_bit_in_parsed_field)
BASE_FEATURE(kAutofillCorrectUserEditedBitInParsedField,
             base::FEATURE_ENABLED_BY_DEFAULT);
// LINT.ThenChange(/components/autofill/ios/form_util/resources/autofill_form_features.ts:autofill_correct_user_edited_bit_in_parsed_field)

// LINT.IfChange(autofill_count_form_submission_in_renderer)
BASE_FEATURE(kAutofillCountFormSubmissionInRenderer,
             base::FEATURE_ENABLED_BY_DEFAULT);
// LINT.ThenChange(//components/autofill/ios/form_util/resources/autofill_form_features.ts:autofill_count_form_submission_in_renderer)

// LINT.IfChange(autofill_dedupe_form_submission)
BASE_FEATURE(kAutofillDedupeFormSubmission, base::FEATURE_ENABLED_BY_DEFAULT);
// LINT.ThenChange(/components/autofill/ios/form_util/resources/autofill_form_features.ts:autofill_dedupe_form_submission)

BASE_FEATURE(kAutofillFixXhrForXframe, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAutofillFormSubmissionEventsInCaptureMode,
             base::FEATURE_ENABLED_BY_DEFAULT);

// LINT.IfChange(autofill_isolated_content_world)
BASE_FEATURE(kAutofillIsolatedWorldForJavascriptIos,
             base::FEATURE_ENABLED_BY_DEFAULT);
// LINT.ThenChange(/components/autofill/ios/form_util/resources/autofill_form_features.ts:autofill_isolated_content_world)

BASE_FEATURE(kAutofillPaymentsSheetV2Ios, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAutofillPaymentsSheetV3Ios, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAutofillRefillForFormsIos, base::FEATURE_DISABLED_BY_DEFAULT);

// LINT.IfChange(autofill_report_form_submission_errors)
BASE_FEATURE(kAutofillReportFormSubmissionErrors,
             base::FEATURE_DISABLED_BY_DEFAULT);
// LINT.ThenChange(/components/autofill/ios/form_util/resources/autofill_form_features.ts:autofill_report_form_submission_errors)

BASE_FEATURE(kAutofillStickyInfobarIos, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAutofillThrottleDocumentFormScanIos,
             base::FEATURE_DISABLED_BY_DEFAULT);
// Minimal period of time between the document form scanning batches.
extern const base::FeatureParam<int> kAutofillDocumentFormScanPeriodMs = {
    &kAutofillThrottleDocumentFormScanIos,
    /*name=*/"period-ms", /*default_value=*/250};

BASE_FEATURE(kAutofillThrottleDocumentFormScanForceFirstScanIos,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAutofillThrottleFilteredDocumentFormScanIos,
             base::FEATURE_DISABLED_BY_DEFAULT);
// Minimal period of time between the filtered document form scanning batches.
extern const base::FeatureParam<int> kAutofillFilteredDocumentFormScanPeriodMs =
    {&kAutofillThrottleFilteredDocumentFormScanIos,
     /*name=*/"period-ms", /*default_value=*/250};
