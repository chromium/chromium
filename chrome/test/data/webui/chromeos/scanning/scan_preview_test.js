// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://scanning/scan_preview.js';

import {assertTrue} from '../../chai_assert.js';

export function scanPreviewTest() {
  /** @type {?ScanPreviewElement} */
  let scanPreview = null;

  setup(() => {
    scanPreview = /** @type {!ScanPreviewElement} */ (
        document.createElement('scan-preview'));
    assertTrue(!!scanPreview);
    document.body.appendChild(scanPreview);
  });

  teardown(() => {
    if (scanPreview) {
      scanPreview.remove();
    }
    scanPreview = null;
  });

  test('initializeScanPreview', () => {
    assertTrue(!!scanPreview.$$('.preview'));
  });
}
