// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility functions to be shared between trusted and untrusted
 * code.
 */

/**
 * Checks if argument is an array with non-zero length.
 * @param {?Object} maybeArray
 * @return {boolean}
 */
export function isNonEmptyArray(maybeArray) {
  return Array.isArray(maybeArray) && maybeArray.length > 0;
}
