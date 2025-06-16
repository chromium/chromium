// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {gCrWebLegacy} from '//ios/web/public/js_messaging/resources/gcrweb.js';

/**
 * @fileoverview Contains feature flag state for behavior relating to Autofill
 *     form extraction and filling. Each entry should correspond to a
 *     base::Feature in C++ land.
 */

// LINT.IfChange(autofill_across_iframes_ios)
/**
 * Whether or not to register and return child frame IDs when extracting forms.
 * Corresponds to autofill::feature::AutofillAcrossIframesIos.
 */
let autofillAcrossIframes: boolean = false;

/**
 * True if the throttling of child frames for autofill across iframes is
 * enabled.
 */
let autofillAcrossIframesThrottling: boolean = false;
// LINT.ThenChange(//components/autofill/core/common/autofill_features.cc:autofill_across_iframes_ios)

// LINT.IfChange(autofill_disallow_slash_dot_labels)
/**
 * True labels must not exclusively contain slashes and dots and other special
 * characters.
 */
let autofillDisallowSlashDotLabels: boolean = true;
// LINT.ThenChange(//components/autofill/core/common/autofill_features.cc:autofill_disallow_slash_dot_labels)

// LINT.IfChange(autofill_isolated_content_world)
/**
 Enables the logic necessary for Autofill to work from an isolated content world
 without breaking the features that need to be in the page content world.
 */
let autofillIsolatedContentWorld: boolean = false;
// LINT.ThenChange(//components/autofill/ios/common/features.mm:autofill_isolated_content_world)

// LINT.IfChange(autofill_correct_user_edited_bit_in_parsed_field)
/**
Enables correctly setting the is_user_edited bit in the parsed form fields
instead of using true by default.
 */
let autofillCorrectUserEditedBitInParsedField: boolean = false;
// LINT.ThenChange(//components/autofill/ios/common/features.mm:autofill_correct_user_edited_bit_in_parsed_field)

// LINT.IfChange(autofill_allow_default_prevented_submission)
/**
Allows detecting form submissions that are `defaultPrevented` by the page
content.
*/
let autofillAllowDefaultPreventedSubmission: boolean = false;
// LINT.ThenChange(//components/autofill/ios/common/features.mm:autofill_allow_default_prevented_submission)

// LINT.IfChange(autofill_dedupe_form_submission)
/**
Dedupes form submission by only allowing one submission per form.
*/
let autofillDedupeFormSubmission: boolean = false;
// LINT.ThenChange(//components/autofill/ios/common/features.mm:autofill_dedupe_form_submission)

// LINT.IfChange(autofill_report_form_submission_errors)
/**
 * Reports JS errors that occur upon handling form submission in the renderer.
 */
let autofillReportFormSubmissionErrors: boolean = false;
// LINT.ThenChange(//components/autofill/ios/common/features.mm:autofill_report_form_submission_errors)

/**
 * @see autofillAcrossIframes
 */
function setAutofillAcrossIframes(enabled: boolean): void {
  autofillAcrossIframes = enabled;
}

/**
 * @see autofillAcrossIframes
 */
function isAutofillAcrossIframesEnabled(): boolean {
  return autofillAcrossIframes;
}

/**
 * @see autofillAcrossIframesThrottling
 */
function setAutofillAcrossIframesThrottling(enabled: boolean): void {
  autofillAcrossIframesThrottling = enabled;
}

/**
 * @see setAutofillAcrossIframesThrottling
 */
function isAutofillAcrossIframesThrottlingEnabled(): boolean {
  return autofillAcrossIframesThrottling;
}

/**
 * @see autofillDisallowSlashDotLabels
 */
function setAutofillDisallowSlashDotLabels(enabled: boolean): void {
  autofillDisallowSlashDotLabels = enabled;
}

/**
 * @see setAutofillDisallowSlashDotLabels
 */
function isAutofillDisallowSlashDotLabelsEnabled(): boolean {
  return autofillDisallowSlashDotLabels;
}

/**
 * @see autofillIsolatedContentWorld
 */
function setAutofillIsolatedContentWorld(enabled: boolean): void {
  autofillIsolatedContentWorld = enabled;
}

/**
 * @see autofillIsolatedContentWorld
 */
function isAutofillIsolatedContentWorldEnabled(): boolean {
  return autofillIsolatedContentWorld;
}

/**
 * @see autofillCorrectUserEditedBitInParsedField
 */
function setAutofillCorrectUserEditedBitInParsedField(enabled: boolean): void {
  autofillCorrectUserEditedBitInParsedField = enabled;
}

/**
 * @see autofillCorrectUserEditedBitInParsedField
 */
function isAutofillCorrectUserEditedBitInParsedField(): boolean {
  return autofillCorrectUserEditedBitInParsedField;
}


/**
 * @see autofillAllowDefaultPreventedSubmission
 */
function setAutofillAllowDefaultPreventedSubmission(enabled: boolean): void {
  autofillAllowDefaultPreventedSubmission = enabled;
}

/**
 * @see autofillAllowDefaultPreventedSubmission
 */
function isAutofillAllowDefaultPreventedSubmission(): boolean {
  return autofillAllowDefaultPreventedSubmission;
}

/**
 * @see autofillDedupeFormSubmission
 */
function setAutofillDedupeFormSubmission(enabled: boolean): void {
  autofillDedupeFormSubmission = enabled;
}

/**
 * @see autofillDedupeFormSubmission
 */
function isAutofillDedupeFormSubmissionEnabled(): boolean {
  return autofillDedupeFormSubmission;
}

/**
 * @see autofillReportFormSubmissionErrors
 */
function setAutofillReportFormSubmissionErrors(enabled: boolean): void {
  autofillReportFormSubmissionErrors = enabled;
}

/**
 * @see autofillReportFormSubmissionErrors
 */
function isAutofillReportFormSubmissionErrorsEnabled(): boolean {
  return autofillReportFormSubmissionErrors;
}

// Expose globally via `gCrWeb` instead of `export` to ensure state (feature
// on/off) is maintained across imports.
gCrWebLegacy.autofill_form_features = {
  setAutofillAcrossIframes,
  isAutofillAcrossIframesEnabled,
  setAutofillAcrossIframesThrottling,
  isAutofillAcrossIframesThrottlingEnabled,
  setAutofillDisallowSlashDotLabels,
  isAutofillDisallowSlashDotLabelsEnabled,
  setAutofillIsolatedContentWorld,
  isAutofillIsolatedContentWorldEnabled,
  setAutofillCorrectUserEditedBitInParsedField,
  isAutofillCorrectUserEditedBitInParsedField,
  setAutofillAllowDefaultPreventedSubmission,
  isAutofillAllowDefaultPreventedSubmission,
  setAutofillDedupeFormSubmission,
  isAutofillDedupeFormSubmissionEnabled,
  setAutofillReportFormSubmissionErrors,
  isAutofillReportFormSubmissionErrorsEnabled,
};
