// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://scanning/scan_preview.js';
import 'chrome://scanning/scanning_app.js';

export function scanPreviewTest() {
  /** @type {?ScanPreviewElement} */
  let scanPreview = null;

  setup(() => {
    scanPreview = document.createElement('scan-preview');
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
