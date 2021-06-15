// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {NetworkType} from './diagnostics_types.js';

/**
 * Converts a KiB storage value to GiB and returns a fixed-point string
 * to the desired number of decimal places.
 * @param {number} value
 * @param {number} numDecimalPlaces
 * @return {string}
 */
export function convertKibToGibDecimalString(value, numDecimalPlaces) {
  return (value / 2 ** 20).toFixed(numDecimalPlaces);
}

/**
 * Returns an icon from the diagnostics icon set.
 * @param {string} id
 * @return {string}
 */
export function getDiagnosticsIcon(id) {
  return `diagnostics:${id}`;
}

/**
 * @param {!NetworkType} type
 * @return {string}
 */
export function getNetworkType(type) {
  // TODO(michaelcheco): Add localized strings.
  switch (type) {
    case NetworkType.kWiFi:
      return 'WiFi';
    case NetworkType.kEthernet:
      return 'Ethernet';
    case NetworkType.kCellular:
      return 'Cellular';
    default:
      assertNotReached();
      return '';
  }
}
