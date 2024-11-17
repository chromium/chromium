// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/common/features.h"

// Features that are exlusive to iOS go here in alphabetical order.

// Controls whether to dynamically load the address input fields in the save
// flow and settings based on the country value.
// TODO(crbug.com/40281788): Remove once launched.
BASE_FEATURE(kAutofillDynamicallyLoadsFieldsForAddressInput,
             "AutofillDynamicallyLoadsFieldsForAddressInput",
             base::FEATURE_DISABLED_BY_DEFAULT);

// LINT.IfChange(autofill_isolated_content_world)
// Controls whether to use the isolated content world instead of the page
// content world for the Autofill JS feature scripts.
// TODO(crbug.com/40747550) Remove once the isolated content world is launched
// for Autofill.
BASE_FEATURE(kAutofillIsolatedWorldForJavascriptIos,
             "AutofillIsolatedWorldForJavascriptIos",
             base::FEATURE_DISABLED_BY_DEFAULT);
// LINT.ThenChange(/components/autofill/ios/form_util/resources/autofill_form_features.ts:autofill_isolated_content_world)

// Enables the second version of the payments suggestion bottom sheet to prevent
// bugs that we've seen in production on other transaction sheets (e.g. some
// fields becoming unresponsive).
BASE_FEATURE(kAutofillPaymentsSheetV2Ios,
             "AutofillPaymentsSheetV2Ios",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Makes the autofill and password infobars sticky on iOS. The sticky infobar
// sticks there until navigating from an explicit user gesture (e.g. reload or
// load a new page from the omnibox). This includes the infobar UI and the
// badge. The badge may remain there after the infobar UI is dismissed from
// timeout but will be dismissed once navigating from an explicit user gesture.
BASE_FEATURE(kAutofillStickyInfobarIos,
             "AutofillStickyInfobarIos",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Throttles the document form scanning done for taking recurrent snaphots of
// the forms in the renderer by using scheduled batches. This doesn't throttle
// single form fetching (aka filtered form fetching), e.g. getting the latest
// snapshot of a form when handling a form activity, which is usually a more
// user visible operation that requires its own special throttling adjustment.
BASE_FEATURE(kAutofillThrottleDocumentFormScanIos,
             "AutofillThrottleDocumentFormScanIos",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Minimal period of time between the document form scanning batches.
extern const base::FeatureParam<int> kAutofillDocumentFormScanPeriodMs = {
    &kAutofillThrottleDocumentFormScanIos,
    /*name=*/"period-ms", /*default_value=*/250};

// Force the first document scan that is triggered when the frame is discovered
// to make the initial form data available as soon as possible to keep the
// status quo with how the initial document scanning was triggered prior to
// batching.
BASE_FEATURE(kAutofillThrottleDocumentFormScanForceFirstScanIos,
             "AutofillThrottleDocumentFormScanForceFirstScanIos",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Throttles the filtered document form scanning done for taking a snapshot of
// specific forms on the spot. Throttles with scheduled batches.
BASE_FEATURE(kAutofillThrottleFilteredDocumentFormScanIos,
             "AutofillThrottleFilteredDocumentFormScanIos",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Minimal period of time between the filtered document form scanning batches.
extern const base::FeatureParam<int> kAutofillFilteredDocumentFormScanPeriodMs =
    {&kAutofillThrottleFilteredDocumentFormScanIos,
     /*name=*/"period-ms", /*default_value=*/250};
