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

// LINT.IfChange(autofill_isolated_content_world)
/**
 Enables the logic necessary for Autofill to work from an isolated content world
 without breaking the features that need to be in the page content world.
 */
let autofillIsolatedContentWorld: boolean = false;
// LINT.ThenChange(//components/autofill/ios/common/features.cc:autofill_isolated_content_world)

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

// Expose globally via `gCrWeb` instead of `export` to ensure state (feature
// on/off) is maintained across imports.
gCrWeb.autofill_form_features = {
  setAutofillAcrossIframes,
  isAutofillAcrossIframesEnabled,
  setAutofillIsolatedContentWorld,
  isAutofillIsolatedContentWorldEnabled,
};
