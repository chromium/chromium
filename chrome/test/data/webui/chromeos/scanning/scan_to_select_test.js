// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning_mojom_imports.js';
import 'chrome://scanning/scan_to_select.js';

import {ScanningBrowserProxyImpl} from 'chrome://scanning/scanning_browser_proxy.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {changeSelect} from './scanning_app_test_utils.js';
import {TestScanningBrowserProxy} from './test_scanning_browser_proxy.js';

suite('scanToSelectTest', function() {
  /** @type {?ScanToSelectElement} */
  let scanToSelect = null;

  /** @type {?TestScanningBrowserProxy} */
  let scanningBrowserProxy = null;

  const myFiles = 'My files';
  const selectFolderText = 'Select folder in Files app…';

  setup(() => {
    scanningBrowserProxy = new TestScanningBrowserProxy();
    ScanningBrowserProxyImpl.setInstance(scanningBrowserProxy);

    scanToSelect = /** @type {!ScanToSelectElement} */ (
        document.createElement('scan-to-select'));
    assertTrue(!!scanToSelect);
    document.body.appendChild(scanToSelect);
  });

  teardown(() => {
    if (scanToSelect) {
      scanToSelect.remove();
    }
    scanToSelect = null;
  });

  // Verifies the 'Scan To' dropdown is initialized enabled with the 'My files'
  // and 'Select folder' option.
  test('initializeScanToSelect', () => {
    const select = scanToSelect.shadowRoot.querySelector('select');
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
    const select =
        /** @type {!HTMLSelectElement} */ (
            scanToSelect.shadowRoot.querySelector('select'));
    return changeSelect(select, /* value */ null, /* selectedIndex */ 1)
        .then(() => {
          assertEquals(myDownloads, scanToSelect.selectedFolder);
          assertEquals(myDownloadsPath, scanToSelect.selectedFilePath);
          assertEquals(
              myDownloads,
              select.options[select.selectedIndex].textContent.trim());
          assertEquals(0, select.selectedIndex);

          scanningBrowserProxy.setSelectedPath(
              {baseName: googleDrive, filePath: googleDrivePath});
          return changeSelect(select, /* value */ null, /* selectedIndex */ 1);
        })
        .then(() => {
          assertEquals(googleDrive, scanToSelect.selectedFolder);
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
    const select =
        /** @type {!HTMLSelectElement} */ (
            scanToSelect.shadowRoot.querySelector('select'));
    return changeSelect(select, /* value */ null, /* selectedIndex */ 1)
        .then(() => {
          assertEquals(myDownloads, scanToSelect.selectedFolder);
          assertEquals(myDownloadsPath, scanToSelect.selectedFilePath);
          assertEquals(
              myDownloads,
              select.options[select.selectedIndex].textContent.trim());
          assertEquals(0, select.selectedIndex);

          // Simulate canceling the select dialog
          scanningBrowserProxy.setSelectedPath({baseName: '', filePath: ''});
          return changeSelect(select, /* value */ null, /* selectedIndex */ 1);
        })
        .then(() => {
          assertEquals(myDownloads, scanToSelect.selectedFolder);
          assertEquals(myDownloadsPath, scanToSelect.selectedFilePath);
          assertEquals(
              myDownloads,
              select.options[select.selectedIndex].textContent.trim());
          assertEquals(0, select.selectedIndex);
        });
  });
});
