// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/chromeos/services/network_health/public/mojom/network_diagnostics.mojom-lite.js';

/**
 * Removes any prefixed URL from a icon image path
 * @param {string} src
 * @return {string}
 */
export function getIconFromSrc(src) {
  const values = src.split('/');
  return values[values.length - 1];
}

/**
 * Creates and returns a basic RoutineResult structure
 * @param {!chromeos.networkDiagnostics.mojom.RoutineVerdict} verdict
 * @return {!chromeos.networkDiagnostics.mojom.RoutineResult}
 */
export function createResult(verdict) {
  return {
    verdict: verdict,
    problems: {},
    timestamp: {
      internalValue: BigInt(0),
    },
  };
}
