// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://scanning/scan_done_section.js';

import {assertEquals, assertTrue} from '../../chai_assert.js';

export function scanDoneSectionTest() {
  /** @type {?ScanDoneSectionElement} */
  let scanDoneSection = null;

  setup(() => {
    scanDoneSection = /** @type {!ScanDoneSectionElement} */ (
        document.createElement('scan-done-section'));
    assertTrue(!!scanDoneSection);
    document.body.appendChild(scanDoneSection);
  });

  teardown(() => {
    if (scanDoneSection) {
      scanDoneSection.remove();
    }
    scanDoneSection = null;
  });

  test('initializeScanDoneSection', () => {
    assertTrue(!!scanDoneSection.$.title);
    assertTrue(!!scanDoneSection.$$('.done-button-container'));
  });

  test('pageNumberUpdatesTitleText', () => {
    scanDoneSection.pageNumber = 1;
    assertEquals(
        'Scanned file saved!', scanDoneSection.$.title.textContent.trim());
    scanDoneSection.pageNumber = 2;
    assertEquals(
        'Scanned files saved!', scanDoneSection.$.title.textContent.trim());
  });
}
