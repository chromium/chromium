// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';
import 'chrome://scanning/scan_to_select.js';

import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {ScanToSelectElement} from 'chrome://scanning/scan_to_select.js';
import {ScanningBrowserProxyImpl} from 'chrome://scanning/scanning_browser_proxy.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {changeSelectedIndex} from './scanning_app_test_utils.js';
import {TestScanningBrowserProxy} from './test_scanning_browser_proxy.js';

suite('scanToSelectTest', function() {
  let scanToSelect: ScanToSelectElement|null = null;

  let scanningBrowserProxy: TestScanningBrowserProxy;

  const myFiles = 'My files';
  const selectFolderText = 'Select folder in Files app…';

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    scanningBrowserProxy = new TestScanningBrowserProxy();
    ScanningBrowserProxyImpl.setInstance(scanningBrowserProxy);

    scanToSelect = document.createElement('scan-to-select');
    assertTrue(!!scanToSelect);
    document.body.appendChild(scanToSelect);
  });

  teardown(() => {
    scanToSelect?.remove();
    scanToSelect = null;
  });

  function getSelect(): HTMLSelectElement {
    assert(scanToSelect);
    const select =
        strictQuery('select', scanToSelect.shadowRoot, HTMLSelectElement);
    assert(select);
    return select;
  }

  function getOption(index: number): HTMLOptionElement {
    const options = Array.from(getSelect().querySelectorAll('option'));
    assert(index < options.length);
    return options[index]!;
  }

  // Verifies the 'Scan To' dropdown is initialized enabled with the 'My files'
  // and 'Select folder' option.
  test('initializeScanToSelect', () => {
    const select = getSelect();
    assertTrue(!!select);
    assertFalse(select.disabled);
    assertEquals(2, select.length);
    assertEquals(myFiles, getOption(0).textContent!.trim());
    assertEquals(selectFolderText, getOption(1).textContent!.trim());
  });

  // Verifies the 'Scan To' dropdown updates when the user chooses a folder in
  // the select dialog.
  test('selectFolderDialog', async () => {
    assert(scanToSelect);
    const googleDrivePath = '/this/is/a/Google/Drive';
    const googleDrive = 'Drive';
    const myDownloadsPath = '/this/is/a/test/directory/My Downloads';
    const myDownloads = 'My Downloads';

    // Simulate clicking the 'Select folder' option.
    scanningBrowserProxy.setSelectedPath(
        {baseName: myDownloads, filePath: myDownloadsPath});
    const select = getSelect();
    await changeSelectedIndex(select, /*index=*/ 1);
    assertEquals(myDownloads, scanToSelect.selectedFolder);
    assertEquals(myDownloadsPath, scanToSelect.selectedFilePath);
    assertEquals(
        myDownloads, getOption(select.selectedIndex).textContent!.trim());
    assertEquals(0, select.selectedIndex);

    scanningBrowserProxy.setSelectedPath(
        {baseName: googleDrive, filePath: googleDrivePath});
    await changeSelectedIndex(select, /*index=*/ 1);
    assertEquals(googleDrive, scanToSelect.selectedFolder);
    assertEquals(googleDrivePath, scanToSelect.selectedFilePath);
    assertEquals(
        googleDrive, getOption(select.selectedIndex).textContent!.trim());
    assertEquals(0, select.selectedIndex);
  });

  // Verifies the 'Scan To' dropdown retains the previous selection when the
  // user cancels the select dialog.
  test('cancelSelectDialog', async () => {
    assert(scanToSelect);
    const myDownloadsPath = '/this/is/a/test/directory/My Downloads';
    const myDownloads = 'My Downloads';

    // Simulate clicking the 'Select folder' option.
    scanningBrowserProxy.setSelectedPath(
        {baseName: myDownloads, filePath: myDownloadsPath});
    const select = getSelect();
    await changeSelectedIndex(select, /*index=*/ 1);
    assertEquals(myDownloads, scanToSelect.selectedFolder);
    assertEquals(myDownloadsPath, scanToSelect.selectedFilePath);
    assertEquals(
        myDownloads, getOption(select.selectedIndex).textContent!.trim());
    assertEquals(0, select.selectedIndex);

    // Simulate canceling the select dialog
    scanningBrowserProxy.setSelectedPath({baseName: '', filePath: ''});
    await changeSelectedIndex(select, /*index=*/ 1);
    assertEquals(myDownloads, scanToSelect.selectedFolder);
    assertEquals(myDownloadsPath, scanToSelect.selectedFilePath);
    assertEquals(
        myDownloads, getOption(select.selectedIndex).textContent!.trim());
    assertEquals(0, select.selectedIndex);
  });
});
