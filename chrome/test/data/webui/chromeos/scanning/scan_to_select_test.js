// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://scanning/scan_preview.js';
import 'chrome://scanning/scanning_app.js';

import {ScanningBrowserProxyImpl} from 'chrome://scanning/scanning_browser_proxy.js';

import {flushTasks} from '../../test_util.m.js';

import {TestScanningBrowserProxy} from './test_scanning_browser_proxy.js';

export function scanToSelectTest() {
  /** @type {?ScanToSelectElement} */
  let scanToSelect = null;

  /** @type {?TestScanningBrowserProxy} */
  let scanningBrowserProxy = null;

  /** @const {string} */
  const myFiles = 'My files';

  /** @const {string} */
  const selectFolderText = 'Select folder in Files app…';

  setup(() => {
    scanningBrowserProxy = new TestScanningBrowserProxy();
    ScanningBrowserProxyImpl.instance_ = scanningBrowserProxy;

    scanToSelect = document.createElement('scan-to-select');
    assertTrue(!!scanToSelect);
    document.body.appendChild(scanToSelect);
  });

  teardown(() => {
    if (scanToSelect) {
      scanToSelect.remove();
    }
    scanToSelect = null;
  });

  test('initializeScanToSelect', () => {
    const select = scanToSelect.$$('select');
    assertTrue(!!select);
    assertFalse(select.disabled);
    assertEquals(2, select.length);
    assertEquals(myFiles, select.options[0].textContent.trim());
    assertEquals(selectFolderText, select.options[1].textContent.trim());
  });

  // Verifies the 'Scan To' dropdown updates when the user chooses a folder in
  // the select dialog.
  test('selectFolderDialog', () => {
    const googleDrivePath = '/this/is/a/Google/Drive';
    const googleDrive = 'Drive';
    const myDownloadsPath = '/this/is/a/test/directory/My Downloads';
    const myDownloads = 'My Downloads';

    // Simulate clicking the 'Select folder' option.
    scanningBrowserProxy.setSelectedPath(
        {baseName: myDownloads, filePath: myDownloadsPath});
    const select = scanToSelect.$$('select');
    select.selectedIndex = 1;
    select.dispatchEvent(new CustomEvent('change'));
    return flushTasks()
        .then(() => {
          assertEquals(myDownloadsPath, scanToSelect.selectedFilePath);
          assertEquals(
              myDownloads,
              select.options[select.selectedIndex].textContent.trim());
          assertEquals(0, select.selectedIndex);

          scanningBrowserProxy.setSelectedPath(
              {baseName: googleDrive, filePath: googleDrivePath});
          select.selectedIndex = 1;
          select.dispatchEvent(new CustomEvent('change'));
          return flushTasks();
        })
        .then(() => {
          assertEquals(googleDrivePath, scanToSelect.selectedFilePath);
          assertEquals(
              googleDrive,
              select.options[select.selectedIndex].textContent.trim());
          assertEquals(0, select.selectedIndex);
        });
  });

  // Verifies the 'Scan To' dropdown retains the previous selection when the
  // user cancels the select dialog.
  test('cancelSelectDialog', () => {
    const myDownloadsPath = '/this/is/a/test/directory/My Downloads';
    const myDownloads = 'My Downloads';

    // Simulate clicking the 'Select folder' option.
    scanningBrowserProxy.setSelectedPath(
        {baseName: myDownloads, filePath: myDownloadsPath});
    const select = scanToSelect.$$('select');
    select.selectedIndex = 1;
    select.dispatchEvent(new CustomEvent('change'));
    return flushTasks()
        .then(() => {
          assertEquals(myDownloadsPath, scanToSelect.selectedFilePath);
          assertEquals(
              myDownloads,
              select.options[select.selectedIndex].textContent.trim());
          assertEquals(0, select.selectedIndex);

          // Simulate canceling the select dialog
          scanningBrowserProxy.setSelectedPath({baseName: '', filePath: ''});
          select.selectedIndex = 1;
          select.dispatchEvent(new CustomEvent('change'));
          return flushTasks();
        })
        .then(() => {
          assertEquals(myDownloadsPath, scanToSelect.selectedFilePath);
          assertEquals(
              myDownloads,
              select.options[select.selectedIndex].textContent.trim());
          assertEquals(0, select.selectedIndex);
        });
  });
}
