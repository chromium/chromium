// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/js/assert.m.js';

/**
 * Converts a chromeos.scanning.mojom.ColorMode string to the corresponding enum
 * value.
 * @param {!string} colorModeString
 * @return {chromeos.scanning.mojom.ColorMode}
 */
export function colorModeFromString(colorModeString) {
  switch (colorModeString) {
    case chromeos.scanning.mojom.ColorMode.kBlackAndWhite.toString():
      return chromeos.scanning.mojom.ColorMode.kBlackAndWhite;
    case chromeos.scanning.mojom.ColorMode.kGrayscale.toString():
      return chromeos.scanning.mojom.ColorMode.kGrayscale;
    case chromeos.scanning.mojom.ColorMode.kColor.toString():
      return chromeos.scanning.mojom.ColorMode.kColor;
    default:
      assertNotReached();
      return chromeos.scanning.mojom.ColorMode.kColor;
  }
}

/**
 * Converts a chromeos.scanning.mojom.ColorMode to a string that can be
 * displayed in the color mode dropdown.
 * @param {number} mojoColorMode
 * @return {!string}
 */
export function getColorModeString(mojoColorMode) {
  // TODO(jschettler): Replace with finalized i18n strings.
  switch (mojoColorMode) {
    case chromeos.scanning.mojom.ColorMode.kBlackAndWhite:
      return 'Black and White';
    case chromeos.scanning.mojom.ColorMode.kGrayscale:
      return 'Grayscale';
    case chromeos.scanning.mojom.ColorMode.kColor:
      return 'Color';
    default:
      assertNotReached();
      return 'Unknown';
  }
}

/**
 * Converts a chromeos.scanning.mojom.SourceType to a string that can be
 * displayed in the source dropdown.
 * @param {number} mojoSourceType
 * @return {!string}
 */
export function getSourceTypeString(mojoSourceType) {
  // TODO(jschettler): Replace with finalized i18n strings.
  switch (mojoSourceType) {
    case chromeos.scanning.mojom.SourceType.kFlatbed:
      return 'Flatbed';
    case chromeos.scanning.mojom.SourceType.kAdfSimplex:
      return 'Document Feeder (Simplex)';
    case chromeos.scanning.mojom.SourceType.kAdfDuplex:
      return 'Document Feeder (Duplex)';
    case chromeos.scanning.mojom.SourceType.kDefault:
      return 'Default';
    case chromeos.scanning.mojom.SourceType.kUnknown:
    default:
      assertNotReached();
      return 'Unknown';
  }
}

/**
 * Converts an unguessable token to a string by combining the high and low
 * values as strings with a hashtag as the separator.
 * @param {!mojoBase.mojom.UnguessableToken} token
 * @return {!string}
 */
export function tokenToString(token) {
  return `${token.high.toString()}#${token.low.toString()}`;
}
