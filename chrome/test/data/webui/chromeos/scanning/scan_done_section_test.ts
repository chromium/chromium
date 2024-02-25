// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';
import 'chrome://scanning/scan_done_section.js';

import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {ScanDoneSectionElement} from 'chrome://scanning/scan_done_section.js';
import {FileType} from 'chrome://scanning/scanning.mojom-webui.js';
import {ScanningBrowserProxyImpl} from 'chrome://scanning/scanning_browser_proxy.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/chromeos/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestScanningBrowserProxy} from './test_scanning_browser_proxy.js';

suite('scanDoneSectionTest', function() {
  let scanDoneSection: ScanDoneSectionElement|null = null;

  let scanningBrowserProxy: TestScanningBrowserProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    scanningBrowserProxy = new TestScanningBrowserProxy();
    ScanningBrowserProxyImpl.setInstance(scanningBrowserProxy);

    scanDoneSection = document.createElement('scan-done-section');
    assertTrue(!!scanDoneSection);
    document.body.appendChild(scanDoneSection);
  });

  teardown(() => {
    scanDoneSection?.remove();
    scanDoneSection = null;
    scanningBrowserProxy.reset();
  });

  // Verify the scan done section can be initialized.
  test('initializeScanDoneSection', () => {
    assert(scanDoneSection);
    assertTrue(!!strictQuery(
        '#doneButtonContainer', scanDoneSection.shadowRoot, HTMLElement));
  });

  // Verify the file saved text updates correctly based on the number of files
  // saved.
  test('numFilesSavedUpdatesFileSavedText', async () => {
    assert(scanDoneSection);
    scanDoneSection.selectedFolder = 'My files';
    scanDoneSection.numFilesSaved = 1;
    await flushTasks();
    const fileSaveText =
        strictQuery('#fileSavedText', scanDoneSection.shadowRoot, HTMLElement);
    assertEquals(
        'Your file has been successfully scanned and saved to My files.',
        fileSaveText.textContent!.trim());
    scanDoneSection.numFilesSaved = 2;
    await flushTasks();
    assertEquals(
        'Your files have been successfully scanned and saved to My ' +
            'files.',
        fileSaveText.textContent!.trim());
  });

  // Verify the file saved text updates correctly based on the selected folder.
  test('selectedFolderUpdatesFileSavedText', async () => {
    assert(scanDoneSection);
    scanDoneSection.selectedFolder = 'Downloads';
    scanDoneSection.numFilesSaved = 1;
    await flushTasks();

    const fileSaveText =
        strictQuery('#fileSavedText', scanDoneSection.shadowRoot, HTMLElement);
    assertEquals(
        'Your file has been successfully scanned and saved to Downloads.',
        fileSaveText.textContent!.trim());

    scanDoneSection.selectedFolder = 'My Drive';
    await flushTasks();

    assertEquals(
        'Your file has been successfully scanned and saved to My Drive.',
        fileSaveText.textContent!.trim());
  });

  // Verify clicking the file location text link invokes showFileInLocation();
  test('showFileLocation', async () => {
    assert(scanDoneSection);
    let fileNotFoundEventFired = false;
    scanDoneSection.addEventListener('file-not-found', function() {
      fileNotFoundEventFired = true;
    });

    const scannedFilePaths: FilePath[] =
        [{'path': '/test/path/scan1.jpg'}, {'path': '/test/path/scan2.jpg'}];
    scanningBrowserProxy.setPathToFile(scannedFilePaths[1]!.path);
    scanDoneSection.scannedFilePaths = scannedFilePaths;
    scanDoneSection.numFilesSaved = 1;
    await flushTasks();
    strictQuery('#folderLink', scanDoneSection.shadowRoot, HTMLAnchorElement)
        .click();
    await flushTasks();
    assertEquals(1, scanningBrowserProxy.getCallCount('showFileInLocation'));
    assertFalse(fileNotFoundEventFired);
  });

  // Verify attempting to open a missing file fires the 'file-not-found' event.
  test('showFileLocationFileNotFound', async () => {
    assert(scanDoneSection);
    let fileNotFoundEventFired = false;
    scanDoneSection.addEventListener('file-not-found', function() {
      fileNotFoundEventFired = true;
    });
    const fileNotFoundEvent = eventToPromise('file-not-found', scanDoneSection);

    scanningBrowserProxy.setPathToFile('/wrong/path/file/so/not/found.jpg');
    scanDoneSection.scannedFilePaths = [{'path': '/test/path/scan.jpg'}];
    scanDoneSection.numFilesSaved = 1;
    await flushTasks();
    strictQuery('#folderLink', scanDoneSection.shadowRoot, HTMLElement).click();
    await fileNotFoundEvent;
    assertEquals(1, scanningBrowserProxy.getCallCount('showFileInLocation'));
    assertTrue(fileNotFoundEventFired);
  });

  // Verify clicking the done button fires the 'done-click' event.
  test('doneClick', async () => {
    assert(scanDoneSection);
    let doneEventFired = false;
    scanDoneSection.addEventListener('done-click', function() {
      doneEventFired = true;
    });
    const doneClickEvent = eventToPromise('done-click', scanDoneSection);

    strictQuery('#doneButton', scanDoneSection.shadowRoot, HTMLElement).click();
    await doneClickEvent;
    assertTrue(doneEventFired);
  });

  // Verify clicking the Show in folder button invokes showFileInLocation().
  test('showInFolderButtonClick', async () => {
    assert(scanDoneSection);
    const scannedFilePaths: FilePath[] =
        [{'path': '/test/path/scan1.jpg'}, {'path': '/test/path/scan2.jpg'}];
    scanningBrowserProxy.setPathToFile(scannedFilePaths[1]!.path);
    scanDoneSection.scannedFilePaths = scannedFilePaths;
    const clickEvent = eventToPromise('click', scanDoneSection);

    await flushTasks();
    strictQuery('#showInFolderButton', scanDoneSection.shadowRoot, HTMLElement)
        .click();
    await clickEvent;
    await flushTasks();
    assertEquals(1, scanningBrowserProxy.getCallCount('showFileInLocation'));
  });

  // Verify clicking the edit button attempts to open the Media app with the
  // correct file paths.
  test('editButtonClick', async () => {
    assert(scanDoneSection);
    const scannedFilePaths =
        [{'path': '/test/path/scan1.jpg'}, {'path': '/test/path/scan2.jpg'}];
    scanDoneSection.scannedFilePaths = scannedFilePaths;
    scanDoneSection.selectedFileType = FileType.kJpg.toString();
    const clickEvent = eventToPromise('click', scanDoneSection);

    strictQuery('#editButton', scanDoneSection.shadowRoot, HTMLElement).click();
    await clickEvent;
    const filePathsSentToMediaApp: string[] =
        scanningBrowserProxy.getArgs('openFilesInMediaApp')[0] as string[];
    assertArrayEquals(
        scannedFilePaths.map(filePath => filePath.path),
        filePathsSentToMediaApp);
  });

  // Verify the edit button label is updated correctly based on the number of
  // saved files.
  test('editFileButtonLabel', async () => {
    assert(scanDoneSection);
    scanDoneSection.numFilesSaved = 1;
    await flushTasks();
    const buttonLabel = strictQuery(
        '#editButtonLabel', scanDoneSection.shadowRoot, HTMLElement);
    assertEquals('Edit file', buttonLabel.textContent!.trim());
    scanDoneSection.numFilesSaved = 2;
    await flushTasks();
    assertEquals('Edit files', buttonLabel.textContent!.trim());
  });
});
