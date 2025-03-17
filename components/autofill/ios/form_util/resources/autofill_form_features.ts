// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

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

// LINT.IfChange(autofill_isolated_content_world)
/**
 Enables the logic necessary for Autofill to work from an isolated content world
 without breaking the features that need to be in the page content world.
 */
let autofillIsolatedContentWorld: boolean = false;
// LINT.ThenChange(//components/autofill/ios/common/features.mm:autofill_isolated_content_world)

// LINT.IfChange(autofill_fix_post_filling_payment_sheet)
/**
Enables fixing the issue where the payment sheet spams after dismissing a
modal dialog that was triggered from the KA (e.g. filling a suggestion).
 */
let autofillFixPaymentSheetSpam: boolean = false;
// LINT.ThenChange(//components/autofill/ios/common/features.mm:autofill_fix_post_filling_payment_sheet)


// LINT.IfChange(autofill_correct_user_edited_bit_in_parsed_field)
/**
Enables correctly setting the is_user_edited bit in the parsed form fields
instead of using true by default.
 */
let autofillCorrectUserEditedBitInParsedField: boolean = false;
// LINT.ThenChange(//components/autofill/ios/common/features.mm:autofill_correct_user_edited_bit_in_parsed_field)

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
 * @see autofillFixPaymentSheetSpam
 */
function setAutofillFixPaymentSheetSpam(enabled: boolean): void {
  autofillFixPaymentSheetSpam = enabled;
}

/**
 * @see autofillFixPaymentSheetSpam
 */
function isAutofillFixPaymentSheetSpamEnabled(): boolean {
  return autofillFixPaymentSheetSpam;
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

// Expose globally via `gCrWeb` instead of `export` to ensure state (feature
// on/off) is maintained across imports.
gCrWeb.autofill_form_features = {
  setAutofillAcrossIframes,
  isAutofillAcrossIframesEnabled,
  setAutofillAcrossIframesThrottling,
  isAutofillAcrossIframesThrottlingEnabled,
  setAutofillIsolatedContentWorld,
  isAutofillIsolatedContentWorldEnabled,
  setAutofillFixPaymentSheetSpam,
  isAutofillFixPaymentSheetSpamEnabled,
  setAutofillCorrectUserEditedBitInParsedField,
  isAutofillCorrectUserEditedBitInParsedField,
};
