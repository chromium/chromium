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
// LINT.ThenChange(//components/autofill/core/common/autofill_features.cc:autofill_across_iframes_ios)

// LINT.IfChange(autofill_xhr_submission_detection_ios)
/**
 * Enables sending all form removal events to the browser for submission detection.
 * Corresponds to autofill::feature::AutofillEnableXHRSubmissionDetectionIOS.
 */
let autofillXHRSubmissionDetection: boolean = false;
// LINT.ThenChange(//components/autofill/core/common/autofill_features.cc:autofill_xhr_submission_detection_ios)

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
 * @see autofillXHRSubmissionDetectionEnabled
 */
function setAutofillXHRSubmissionDetection(enabled: boolean): void {
  autofillXHRSubmissionDetection = enabled;
}

/**
 * @see autofillXHRSubmissionDetection
 */
function isAutofillXHRSubmissionDetectionEnabled(): boolean {
  return autofillXHRSubmissionDetection;
}

// Expose globally via `gCrWeb` instead of `export` to ensure state (feature
// on/off) is maintained across imports.
gCrWeb.autofill_form_features = {
  setAutofillAcrossIframes,
  isAutofillAcrossIframesEnabled,
  setAutofillXHRSubmissionDetection,
  isAutofillXHRSubmissionDetectionEnabled,
};
