// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning_mojom_imports.js';
import 'chrome://scanning/scan_done_section.js';

import {FileType} from 'chrome://scanning/scanning.mojom-webui.js';
import {ScanningBrowserProxyImpl} from 'chrome://scanning/scanning_browser_proxy.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {isVisible} from 'chrome://webui-test/chromeos/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestScanningBrowserProxy} from './test_scanning_browser_proxy.js';

suite('scanDoneSectionTest', function() {
  /** @type {?ScanDoneSectionElement} */
  let scanDoneSection = null;

  /** @type {?TestScanningBrowserProxy} */
  let scanningBrowserProxy = null;

  setup(() => {
    scanningBrowserProxy = new TestScanningBrowserProxy();
    ScanningBrowserProxyImpl.setInstance(scanningBrowserProxy);

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
    scanningBrowserProxy.reset();
  });

  // Verify the scan done section can be initialized.
  test('initializeScanDoneSection', () => {
    assertTrue(
        !!scanDoneSection.shadowRoot.querySelector('#doneButtonContainer'));
  });

  // Verify the file saved text updates correctly based on the number of files
  // saved.
  test('numFilesSavedUpdatesFileSavedText', () => {
    scanDoneSection.selectedFolder = 'My files';
    scanDoneSection.numFilesSaved = 1;
    return flushTasks()
        .then(() => {
          assertEquals(
              'Your file has been successfully scanned and saved to My files.',
              scanDoneSection.shadowRoot.querySelector('#fileSavedText')
                  .textContent.trim());
          scanDoneSection.numFilesSaved = 2;
          return flushTasks();
        })
        .then(() => {
          assertEquals(
              'Your files have been successfully scanned and saved to My ' +
                  'files.',
              scanDoneSection.shadowRoot.querySelector('#fileSavedText')
                  .textContent.trim());
        });
  });

  // Verify the file saved text updates correctly based on the selected folder.
  test('selectedFolderUpdatesFileSavedText', () => {
    scanDoneSection.selectedFolder = 'Downloads';
    scanDoneSection.numFilesSaved = 1;
    return flushTasks()
        .then(() => {
          assertEquals(
              'Your file has been successfully scanned and saved to Downloads.',
              scanDoneSection.shadowRoot.querySelector('#fileSavedText')
                  .textContent.trim());
          scanDoneSection.selectedFolder = 'My Drive';
          return flushTasks();
        })
        .then(() => {
          assertEquals(
              'Your file has been successfully scanned and saved to My Drive.',
              scanDoneSection.shadowRoot.querySelector('#fileSavedText')
                  .textContent.trim());
        });
  });

  // Verify clicking the file location text link invokes showFileInLocation();
  test('showFileLocation', () => {
    let fileNotFoundEventFired = false;
    scanDoneSection.addEventListener('file-not-found', function() {
      fileNotFoundEventFired = true;
    });

    const scannedFilePaths =
        [{'path': '/test/path/scan1.jpg'}, {'path': '/test/path/scan2.jpg'}];
    scanningBrowserProxy.setPathToFile(scannedFilePaths[1].path);
    scanDoneSection.scannedFilePaths = scannedFilePaths;
    scanDoneSection.numFilesSaved = 1;
    return flushTasks().then(() => {
      scanDoneSection.shadowRoot.querySelector('#folderLink').click();
      return flushTasks().then(() => {
        assertEquals(
            1, scanningBrowserProxy.getCallCount('showFileInLocation'));
        assertFalse(fileNotFoundEventFired);
      });
    });
  });

  // Verify attempting to open a missing file fires the 'file-not-found' event.
  test('showFileLocationFileNotFound', () => {
    let fileNotFoundEventFired = false;
    scanDoneSection.addEventListener('file-not-found', function() {
      fileNotFoundEventFired = true;
    });

    scanningBrowserProxy.setPathToFile('/wrong/path/file/so/not/found.jpg');
    scanDoneSection.scannedFilePaths = [{'path': '/test/path/scan.jpg'}];
    scanDoneSection.numFilesSaved = 1;
    return flushTasks().then(() => {
      scanDoneSection.shadowRoot.querySelector('#folderLink').click();
      return flushTasks().then(() => {
        assertEquals(
            1, scanningBrowserProxy.getCallCount('showFileInLocation'));
        assertTrue(fileNotFoundEventFired);
      });
    });
  });

  // Verify clicking the done button fires the 'done-click' event.
  test('doneClick', () => {
    let doneEventFired = false;
    scanDoneSection.addEventListener('done-click', function() {
      doneEventFired = true;
    });

    scanDoneSection.shadowRoot.querySelector('#doneButton').click();
    assertTrue(doneEventFired);
  });

  // Verify clicking the Show in folder button invokes showFileInLocation().
  test('showInFolderButtonClick', () => {
    const scannedFilePaths =
        [{'path': '/test/path/scan1.jpg'}, {'path': '/test/path/scan2.jpg'}];
    scanningBrowserProxy.setPathToFile(scannedFilePaths[1].path);
    scanDoneSection.scannedFilePaths = scannedFilePaths;

    return flushTasks().then(() => {
      scanDoneSection.shadowRoot.querySelector('#showInFolderButton').click();
      return flushTasks().then(() => {
        assertEquals(
            1, scanningBrowserProxy.getCallCount('showFileInLocation'));
      });
    });
  });

  // Verify clicking the edit button attempts to open the Media app with the
  // correct file paths.
  test('editButtonClick', () => {
    const scannedFilePaths =
        [{'path': '/test/path/scan1.jpg'}, {'path': '/test/path/scan2.jpg'}];
    scanDoneSection.scannedFilePaths = scannedFilePaths;
    scanDoneSection.selectedFileType = FileType.kJpg.toString();

    scanDoneSection.shadowRoot.querySelector('#editButton').click();
    const filePathsSentToMediaApp = /** @type {!Array<string>} */ (
        scanningBrowserProxy.getArgs('openFilesInMediaApp')[0]);
    assertArrayEquals(
        scannedFilePaths.map(filePath => filePath.path),
        filePathsSentToMediaApp);
  });

  // Verify the edit button is hidden for the PDF file type because the Media
  // app doesn't support PDFs.
  test('editButtonHiddenForFileTypePdf', () => {
    const editButton =
        /** @type {!HTMLElement} */ (
            scanDoneSection.shadowRoot.querySelector('#editButton'));
    scanDoneSection.selectedFileType = FileType.kPng.toString();
    assertTrue(isVisible(editButton));

    scanDoneSection.selectedFileType = FileType.kPdf.toString();
    assertFalse(isVisible(editButton));
  });

  // Verify the edit button label is updated correctly based on the number of
  // saved files.
  test('editFileButtonLabel', () => {
    scanDoneSection.numFilesSaved = 1;
    return flushTasks()
        .then(() => {
          assertEquals(
              'Edit file',
              scanDoneSection.shadowRoot.querySelector('#editButtonLabel')
                  .textContent.trim());
          scanDoneSection.numFilesSaved = 2;
          return flushTasks();
        })
        .then(() => {
          assertEquals(
              'Edit files',
              scanDoneSection.shadowRoot.querySelector('#editButtonLabel')
                  .textContent.trim());
        });
  });
});
