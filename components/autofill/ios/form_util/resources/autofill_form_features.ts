// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

/**
 * @fileoverview Contains feature flag state for behavior relating to Autofill
 *     form extraction and filling. Each entry should correspond to a
 *     base::Feature in C++ and a placeholder that is dynamically replaced by
 *     C++ at script injection time.
 */

declare const gCrWebPlaceholderAutofillAcrossIframesEnabled: boolean;
declare const gCrWebPlaceholderAutofillAcrossIframesThrottling: boolean;
declare const gCrWebPlaceholderAutofillDisallowMoreHyphenLikeLabels: boolean;
declare const gCrWebPlaceholderAutofillIgnoreCheckableElements: boolean;
declare const gCrWebPlaceholderAutofillSupportDateInput: boolean;
declare const gCrWebPlaceholderAutofillCorrectUserEditedBitInParsedField:
    boolean;
declare const gCrWebPlaceholderAutofillAllowDefaultPreventedSubmission: boolean;
declare const gCrWebPlaceholderAutofillDedupeFormSubmission: boolean;
declare const gCrWebPlaceholderAutofillEmailVerification: boolean;
declare const gCrWebPlaceholderAutofillReportFormSubmissionErrors: boolean;
declare const gCrWebPlaceholderAutofillCountFormSubmissionInRenderer: boolean;
declare const gCrWebPlaceholderAutofillTrackPasswordFieldsIos: boolean;

// LINT.IfChange(autofill_across_iframes_ios)
/**
 * Whether or not to register and return child frame IDs when extracting forms.
 * Corresponds to autofill::features::kAutofillAcrossIframesIos.
 */
function isAutofillAcrossIframesEnabled(): boolean {
  return gCrWebPlaceholderAutofillAcrossIframesEnabled;
}

/**
 * True if the throttling of child frames for autofill across iframes is
 * enabled.
 */
function isAutofillAcrossIframesThrottlingEnabled(): boolean {
  return gCrWebPlaceholderAutofillAcrossIframesThrottling;
}
// LINT.ThenChange(//components/autofill/core/common/autofill_features.cc:autofill_across_iframes_ios)

// LINT.IfChange(autofill_disallow_more_hyphen_like_labels)
/**
 * When true, labels that only contain em dashes, minuses, fullwidth hyphens
 * and other special characters are disallowed.
 */
function isAutofillDisallowMoreHyphenLikeLabelsEnabled(): boolean {
  return gCrWebPlaceholderAutofillDisallowMoreHyphenLikeLabels;
}
// LINT.ThenChange(//components/autofill/core/common/autofill_features.cc:autofill_disallow_more_hyphen_like_labels)

// LINT.IfChange(autofill_ignore_checkable_elements)
/**
 * If true, checkboxes and radio buttons aren't extracted anymore.
 */
function isAutofillIgnoreCheckableElementsEnabled(): boolean {
  return gCrWebPlaceholderAutofillIgnoreCheckableElements;
}
// LINT.ThenChange(//components/autofill/core/common/autofill_features.cc:autofill_ignore_checkable_elements)

// LINT.IfChange(autofill_support_date_input)
/**
 * If true, support for <input type="date"> fields is enabled.
 */
function isAutofillSupportDateInputEnabled(): boolean {
  return gCrWebPlaceholderAutofillSupportDateInput;
}
// LINT.ThenChange(//components/autofill/ios/common/features.mm:autofill_support_date_input)

// LINT.IfChange(autofill_correct_user_edited_bit_in_parsed_field)
/**
 * Enables correctly setting the is_user_edited_deprecated bit in the parsed
 * form fields instead of using true by default.
 */
function isAutofillCorrectUserEditedBitInParsedField(): boolean {
  return gCrWebPlaceholderAutofillCorrectUserEditedBitInParsedField;
}
// LINT.ThenChange(//components/autofill/ios/common/features.mm:autofill_correct_user_edited_bit_in_parsed_field)

// LINT.IfChange(autofill_allow_default_prevented_submission)
/**
 * Allows detecting form submissions that are `defaultPrevented` by the page
 * content.
 */
function isAutofillAllowDefaultPreventedSubmission(): boolean {
  return gCrWebPlaceholderAutofillAllowDefaultPreventedSubmission;
}
// LINT.ThenChange(//components/autofill/ios/common/features.mm:autofill_allow_default_prevented_submission)

// LINT.IfChange(autofill_dedupe_form_submission)
/**
 * Dedupes form submission by only allowing one submission per form.
 */
function isAutofillDedupeFormSubmissionEnabled(): boolean {
  return gCrWebPlaceholderAutofillDedupeFormSubmission;
}
// LINT.ThenChange(//components/autofill/ios/common/features.mm:autofill_dedupe_form_submission)

// LINT.IfChange(autofill_email_verification)
/**
 * Whether or not the Email Verification Protocol is enabled.
 */
function isAutofillEmailVerificationEnabled(): boolean {
  return gCrWebPlaceholderAutofillEmailVerification;
}
// LINT.ThenChange(//components/autofill/ios/common/features.mm:autofill_email_verification)

// LINT.IfChange(autofill_report_form_submission_errors)
/**
 * Reports JS errors that occur upon handling form submission in the renderer.
 */
function isAutofillReportFormSubmissionErrorsEnabled(): boolean {
  return gCrWebPlaceholderAutofillReportFormSubmissionErrors;
}
// LINT.ThenChange(//components/autofill/ios/common/features.mm:autofill_report_form_submission_errors)

// LINT.IfChange(autofill_count_form_submission_in_renderer)
/**
 * Record form submissions events that are detected in the renderer before they
 * are processed.
 */
function isAutofillCountFormSubmissionInRendererEnabled(): boolean {
  return gCrWebPlaceholderAutofillCountFormSubmissionInRenderer;
}
// LINT.ThenChange(//components/autofill/ios/common/features.mm:autofill_count_form_submission_in_renderer)

// LINT.IfChange(autofill_track_password_fields_ios)
/**
 * Whether or not tracking password fields is enabled.
 * Corresponds to kAutofillTrackPasswordFieldsIos.
 */
function isAutofillTrackPasswordFieldsEnabled(): boolean {
  return gCrWebPlaceholderAutofillTrackPasswordFieldsIos;
}
// LINT.ThenChange(//components/autofill/ios/common/features.mm:autofill_track_password_fields_ios)

// Expose globally via `gCrWeb` under the 'autofill_form_features' API name.
const autofillFormFeatures = new CrWebApi('autofill_form_features');

autofillFormFeatures.addFunction(
    'isAutofillAcrossIframesEnabled', isAutofillAcrossIframesEnabled);
autofillFormFeatures.addFunction(
    'isAutofillAcrossIframesThrottlingEnabled',
    isAutofillAcrossIframesThrottlingEnabled);
autofillFormFeatures.addFunction(
    'isAutofillDisallowMoreHyphenLikeLabelsEnabled',
    isAutofillDisallowMoreHyphenLikeLabelsEnabled);
autofillFormFeatures.addFunction(
    'isAutofillIgnoreCheckableElementsEnabled',
    isAutofillIgnoreCheckableElementsEnabled);
autofillFormFeatures.addFunction(
    'isAutofillSupportDateInputEnabled', isAutofillSupportDateInputEnabled);
autofillFormFeatures.addFunction(
    'isAutofillCorrectUserEditedBitInParsedField',
    isAutofillCorrectUserEditedBitInParsedField);
autofillFormFeatures.addFunction(
    'isAutofillAllowDefaultPreventedSubmission',
    isAutofillAllowDefaultPreventedSubmission);
autofillFormFeatures.addFunction(
    'isAutofillDedupeFormSubmissionEnabled',
    isAutofillDedupeFormSubmissionEnabled);
autofillFormFeatures.addFunction(
    'isAutofillEmailVerificationEnabled', isAutofillEmailVerificationEnabled);
autofillFormFeatures.addFunction(
    'isAutofillReportFormSubmissionErrorsEnabled',
    isAutofillReportFormSubmissionErrorsEnabled);
autofillFormFeatures.addFunction(
    'isAutofillCountFormSubmissionInRendererEnabled',
    isAutofillCountFormSubmissionInRendererEnabled);
autofillFormFeatures.addFunction(
    'isAutofillTrackPasswordFieldsEnabled',
    isAutofillTrackPasswordFieldsEnabled);

gCrWeb.registerApi(autofillFormFeatures);
