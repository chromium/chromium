// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://scanning/scan_preview.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AppState} from 'chrome://scanning/scanning_app_types.js';

import {assertEquals, assertTrue} from '../../chai_assert.js';
import {isVisible} from '../../test_util.m.js';

export function scanPreviewTest() {
  /** @type {?ScanPreviewElement} */
  let scanPreview = null;


  /** @type {!HTMLElement} */
  let helpOrProgress;
  /** @type {!HTMLElement} */
  let helperText;
  /** @type {!HTMLElement} */
  let scanProgress;
  /** @type {!HTMLElement} */
  let scannedImages;
  /** @type {!HTMLElement} */
  let cancelingProgress;

  setup(() => {
    scanPreview = /** @type {!ScanPreviewElement} */ (
        document.createElement('scan-preview'));
    assertTrue(!!scanPreview);
    document.body.appendChild(scanPreview);

    helpOrProgress =
        /** @type {!HTMLElement} */ (scanPreview.$$('#helpOrProgress'));
    helperText =
        /** @type {!HTMLElement} */ (scanPreview.$$('#helperText'));
    scanProgress =
        /** @type {!HTMLElement} */ (scanPreview.$$('#scanProgress'));
    scannedImages =
        /** @type {!HTMLElement} */ (scanPreview.$$('#scannedImages'));
    cancelingProgress =
        /** @type {!HTMLElement} */ (scanPreview.$$('#cancelingProgress'));
  });

  teardown(() => {
    if (scanPreview) {
      scanPreview.remove();
    }
    scanPreview = null;
  });

  /**
   * @param {boolean} isHelpOrProgressVisible
   * @param {boolean} isHelperTextVisible
   * @param {boolean} isScanProgressVisible
   * @param {boolean} isScannedImagesVisible
   * @param {boolean} isCancelingProgressVisible
   */
  function assertVisible(
      isHelpOrProgressVisible, isHelperTextVisible, isScanProgressVisible,
      isScannedImagesVisible, isCancelingProgressVisible) {
    assertEquals(isHelpOrProgressVisible, isVisible(helpOrProgress));
    assertEquals(isHelperTextVisible, isVisible(helperText));
    assertEquals(isScanProgressVisible, isVisible(scanProgress));
    assertEquals(isScannedImagesVisible, isVisible(scannedImages));
    assertEquals(isCancelingProgressVisible, isVisible(cancelingProgress));
  }

  test('initializeScanPreview', () => {
    assertTrue(!!scanPreview.$$('.preview'));
  });

  // Test that the progress text updates when the page number increases.
  test('progressTextUpdates', () => {
    scanPreview.appState = AppState.SCANNING;
    scanPreview.pageNumber = 1;
    assertEquals(
        scanPreview.i18n('scanPreviewProgressText', 1),
        scanPreview.$$('#progressText').textContent.trim());
    scanPreview.pageNumber = 2;
    assertEquals(
        scanPreview.i18n('scanPreviewProgressText', 2),
        scanPreview.$$('#progressText').textContent.trim());
  });

  // Tests that the correct element is showing in the preview pane depending on
  // current app state.
  test('appStateTransitions', () => {
    scanPreview.appState = AppState.GETTING_SCANNERS;
    flush();
    assertVisible(
        /*isHelpOrProgressVisible*/ true, /*isHelperTextVisible*/ true,
        /*isScanProgressVisible*/ false, /*isScannedImagesVisible*/ false,
        /*isCancelingProgressVisible*/ false);

    scanPreview.appState = AppState.GOT_SCANNERS;
    flush();
    assertVisible(
        /*isHelpOrProgressVisible*/ true, /*isHelperTextVisible*/ true,
        /*isScanProgressVisible*/ false, /*isScannedImagesVisible*/ false,
        /*isCancelingProgressVisible*/ false);

    scanPreview.appState = AppState.GETTING_CAPS;
    flush();
    assertVisible(
        /*isHelpOrProgressVisible*/ true, /*isHelperTextVisible*/ true,
        /*isScanProgressVisible*/ false, /*isScannedImagesVisible*/ false,
        /*isCancelingProgressVisible*/ false);

    scanPreview.appState = AppState.READY;
    flush();
    assertVisible(
        /*isHelpOrProgressVisible*/ true, /*isHelperTextVisible*/ true,
        /*isScanProgressVisible*/ false, /*isScannedImagesVisible*/ false,
        /*isCancelingProgressVisible*/ false);

    scanPreview.appState = AppState.SCANNING;
    flush();
    assertVisible(
        /*isHelpOrProgressVisible*/ true, /*isHelperTextVisible*/ false,
        /*isScanProgressVisible*/ true, /*isScannedImagesVisible*/ false,
        /*isCancelingProgressVisible*/ false);

    scanPreview.appState = AppState.CANCELING;
    flush();
    assertVisible(
        /*isHelpOrProgressVisible*/ true, /*isHelperTextVisible*/ false,
        /*isScanProgressVisible*/ false, /*isScannedImagesVisible*/ false,
        /*isCancelingProgressVisible*/ true);

    scanPreview.objectUrls = ['image'];
    scanPreview.appState = AppState.DONE;
    flush();
    assertVisible(
        /*isHelpOrProgressVisible*/ false, /*isHelperTextVisible*/ false,
        /*isScanProgressVisible*/ false, /*isScannedImagesVisible*/ true,
        /*isCancelingProgressVisible*/ false);
  });
}
