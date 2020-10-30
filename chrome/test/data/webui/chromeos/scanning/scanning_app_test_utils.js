// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {!mojoBase.mojom.UnguessableToken} id
 * @param {string} displayName
 * @return {!chromeos.scanning.mojom.Scanner}
 */
export function createScanner(id, displayName) {
  return {id, 'displayName': strToMojoString16(displayName)};
}

/**
 * @param {number} type
 * @param {string} name
 * @param {!Array<chromeos.scanning.mojom.PageSize>} pageSizes
 * @return {!chromeos.scanning.mojom.ScanSource}
 */
export function createScannerSource(type, name, pageSizes) {
  return {type, name, pageSizes};
}

/**
 * Converts a JS string to a mojo_base::mojom::String16 object.
 * @param {string} str
 * @return {!mojoBase.mojom.String16}
 */
export function strToMojoString16(str) {
  let arr = [];
  for (var i = 0; i < str.length; i++) {
    arr[i] = str.charCodeAt(i);
  }

  return {data: arr};
}