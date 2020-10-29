// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * This file is only to avoid syntax error from closure compiler since it does
 * not support dynamic imports currently. Once it supports, we should remove
 * this file and import it directly.
 */

/**
 * Imports module dynamically.
 * @param {string} scriptUrl The URL of the script.
 * @return {!Promise}
 */
export async function dynamicImport(scriptUrl) {
  return import(scriptUrl);
}
