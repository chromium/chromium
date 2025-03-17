// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_COMMON_FEATURES_H_
#define COMPONENTS_AUTOFILL_IOS_COMMON_FEATURES_H_

#import "base/feature_list.h"
#import "base/metrics/field_trial_params.h"

// Feature flag to control whether the Add Address Manually bottom sheet is
// enabled.
BASE_DECLARE_FEATURE(kAddAddressManually);

// Returns true if the AddAddressManually feature is enabled
bool IsAddAddressManuallyEnabled();

// Enables correctly setting the is_user_edited bit in the parsed form fields
// instead of using true by default.
BASE_DECLARE_FEATURE(kAutofillCorrectUserEditedBitInParsedField);

// Controls whether to dynamically load the address input fields in the save
// flow and settings based on the country value.
// TODO(crbug.com/40281788): Remove once launched.
BASE_DECLARE_FEATURE(kAutofillDynamicallyLoadsFieldsForAddressInput);

// Enables fixing the issue where the payment sheet spams after dismissing a
// modal dialog that was triggered from the KA (e.g. filling a suggestion).
BASE_DECLARE_FEATURE(kAutofillFixPaymentSheetSpam);

// Controls whether to use the isolated content world instead of the page
// content world for the Autofill JS feature scripts.
// TODO(crbug.com/40747550) Remove once the isolated content world is launched
// for Autofill.
BASE_DECLARE_FEATURE(kAutofillIsolatedWorldForJavascriptIos);

// Enables the second version of the payments suggestion bottom sheet to prevent
// bugs that we've seen in production on other transaction sheets (e.g. some
// fields becoming unresponsive).
BASE_DECLARE_FEATURE(kAutofillPaymentsSheetV2Ios);

// Enables the 3rd version of the payments suggestion bottom sheet that can
// directly pick the Autofill suggestions provider instead of intermediating via
// the FormSuggestionController.
BASE_DECLARE_FEATURE(kAutofillPaymentsSheetV3Ios);

// Makes the autofill and password infobars sticky on iOS. The sticky infobar
// sticks there until navigating from an explicit user gesture (e.g. reload or
// load a new page from the omnibox). This includes the infobar UI and the
// badge. The badge may remain there after the infobar UI is dismissed from
// timeout but will be dismissed once navigating from an explicit user gesture.
BASE_DECLARE_FEATURE(kAutofillStickyInfobarIos);

// Throttles the document form scanning done for taking recurrent snapshots of
// the forms in the renderer by using scheduled batches. This doesn't throttle
// single form fetching (aka filtered form fetching), e.g. getting the latest
// snapshot of a form when handling a form activity, which is usually a more
// user visible operation that requires its own special throttling adjustment.
BASE_DECLARE_FEATURE(kAutofillThrottleDocumentFormScanIos);
extern const base::FeatureParam<int> kAutofillDocumentFormScanPeriodMs;

// Force the first document scan that is triggered when the frame is discovered
// to make the initial form data available as soon as possible to keep the
// status quo with how the initial document scanning was triggered prior to
// batching.
BASE_DECLARE_FEATURE(kAutofillThrottleDocumentFormScanForceFirstScanIos);

// Throttles the filtered document form scanning done for taking a snapshot of
// specific forms on the spot. Throttles with scheduled batches.
BASE_DECLARE_FEATURE(kAutofillThrottleFilteredDocumentFormScanIos);
extern const base::FeatureParam<int> kAutofillFilteredDocumentFormScanPeriodMs;

#endif  // COMPONENTS_AUTOFILL_IOS_COMMON_FEATURES_H_
