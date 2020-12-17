// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://scanning/scan_done_section.js';

import {ScanningBrowserProxyImpl} from 'chrome://scanning/scanning_browser_proxy.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import {TestScanningBrowserProxy} from './test_scanning_browser_proxy.js';

export function scanDoneSectionTest() {
  /** @type {?ScanDoneSectionElement} */
  let scanDoneSection = null;

  /** @type {?TestScanningBrowserProxy} */
  let scanningBrowserProxy = null;

  setup(() => {
    scanningBrowserProxy = new TestScanningBrowserProxy();
    ScanningBrowserProxyImpl.instance_ = scanningBrowserProxy;

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
    assertTrue(!!scanDoneSection.$$('#doneButtonContainer'));
  });

  test('pageNumberUpdatesTitleText', () => {
    scanDoneSection.pageNumber = 1;
    return flushTasks()
        .then(() => {
          assertEquals(
              'Scanned file saved!',
              scanDoneSection.$.title.textContent.trim());
          scanDoneSection.pageNumber = 2;
          return flushTasks();
        })
        .then(() => {
          assertEquals(
              'Scanned files saved!',
              scanDoneSection.$.title.textContent.trim());
        });
  });

  test('showFileLocation', () => {
    let fileNotFoundEventFired = false;
    scanDoneSection.addEventListener('file-not-found', function() {
      fileNotFoundEventFired = true;
    });

    const lastScannedFilePath = {'path': '/test/path/scan.jpg'};
    scanningBrowserProxy.setPathToFile(lastScannedFilePath.path);
    scanDoneSection.lastScannedFilePath = lastScannedFilePath;
    scanDoneSection.$$('#showFileButton').click();
    return flushTasks().then(() => {
      assertEquals(1, scanningBrowserProxy.getCallCount('showFileInLocation'));
      assertFalse(fileNotFoundEventFired);
    });
  });

  test('showFileLocationFileNotFound', () => {
    let fileNotFoundEventFired = false;
    scanDoneSection.addEventListener('file-not-found', function() {
      fileNotFoundEventFired = true;
    });

    scanningBrowserProxy.setPathToFile('/wrong/path/file/so/not/found.jpg');
    scanDoneSection.lastScannedFilePath = {'path': '/test/path/scan.jpg'};
    scanDoneSection.$$('#showFileButton').click();
    return flushTasks().then(() => {
      assertEquals(1, scanningBrowserProxy.getCallCount('showFileInLocation'));
      assertTrue(fileNotFoundEventFired);
    });
  });
}
