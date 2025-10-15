// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_COMMON_FEATURES_H_
#define COMPONENTS_AUTOFILL_IOS_COMMON_FEATURES_H_

#import "base/feature_list.h"
#import "base/metrics/field_trial_params.h"

// Features that are exclusive to iOS go here in alphabetical order.

// Allows detecting form submissions that are `defaultPrevented` by the page
// content.
BASE_DECLARE_FEATURE(kAutofillAllowDefaultPreventedSubmission);

// Enables correctly setting the is_user_edited bit in the parsed form fields
// instead of using true by default.
BASE_DECLARE_FEATURE(kAutofillCorrectUserEditedBitInParsedField);

// Record form submissions events that are detected in the renderer before they
// are processed. Act as a killswitch where the feature is enabled by default.
BASE_DECLARE_FEATURE(kAutofillCountFormSubmissionInRenderer);

// Dedupes form submission by only allowing one submission per form element.
// This feature is meant to be used when preventDefault()ed submit events are
// allowed (i.e. AutofillAllowDefaultPreventedSubmission enabled) which can
// allow submitting the form multiple times as post-submit navigation can be
// prevented.
BASE_DECLARE_FEATURE(kAutofillDedupeFormSubmission);

// Fix for XHR form submission detection when autofill across iframes is
// enabled.
BASE_DECLARE_FEATURE(kAutofillFixXhrForXframe);

// Listen to form submission events in capture mode before the events are
// propagated.
BASE_DECLARE_FEATURE(kAutofillFormSubmissionEventsInCaptureMode);

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

// Enables the refill functionality to allow autofilling of dynamically
// expanding forms.
BASE_DECLARE_FEATURE(kAutofillRefillForFormsIos);

// Reports JS errors that occur upon handling form submission in the renderer.
BASE_DECLARE_FEATURE(kAutofillReportFormSubmissionErrors);

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
