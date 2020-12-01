// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Enum for the state of `scanning-app`.
 * @enum {number}
 */
export const AppState = {
  GETTING_SCANNERS: 0,
  GOT_SCANNERS: 1,
  GETTING_CAPS: 2,
  READY: 3,
  SCANNING: 4,
  DONE: 5,
  CANCELING: 6,
};

/**
 * @typedef {!Array<!chromeos.scanning.mojom.Scanner>}
 */
export let ScannerArr;
