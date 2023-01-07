// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RoutineResult, RoutineVerdict} from 'chrome://resources/mojo/chromeos/services/network_health/public/mojom/network_diagnostics.mojom-webui.js';

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
 * @param {!RoutineVerdict} verdict
 * @return {!RoutineResult}
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
