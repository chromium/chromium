// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/pdf/elements/viewer-zoom-toolbar.js';
import {FittingType} from 'chrome://print/pdf/constants.js';
import {assert} from 'chrome://resources/js/assert.m.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {eventToPromise} from '../test_util.m.js';

window.pdf_zoom_toolbar_test = {};
pdf_zoom_toolbar_test.suiteName = 'PdfToolbarManagerTest';
/** @enum {string} */
pdf_zoom_toolbar_test.TestNames = {
  Toggle: 'toggle',
  ForceFitToPage: 'force fit to page',
};

suite(pdf_zoom_toolbar_test.suiteName, function() {
  /** @type {!ViewerZoomToolbarElement} */
  let zoomToolbar;

  /** @type {!ViewerZoomButtonElement} */
  let fitButton;

  /** @type {!CrIconButtonElement} */
  let button;

  const fitWidthIcon = 'fullscreen';
  const fitPageIcon = 'fullscreen-exit';

  setup(function() {
    document.body.innerHTML = '';

    zoomToolbar = /** @type {!ViewerZoomToolbarElement} */ (
        document.createElement('viewer-zoom-toolbar'));
    document.body.appendChild(zoomToolbar);

    fitButton =
        /** @type {!ViewerZoomButtonElement} */ (zoomToolbar.$['fit-button']);
    button =
        /** @type {!CrIconButtonElement} */ (fitButton.$$('cr-icon-button'));
  });

  /**
   * Test that the zoom toolbar toggles between showing the fit-to-page and
   * fit-to-width buttons.
   */
  test(assert(pdf_zoom_toolbar_test.TestNames.Toggle), async () => {
    // Initial: Show fit-to-page.
    assertTrue(button.ironIcon.endsWith(fitPageIcon));

    // Tap 1: Fire fit-to-changed(FIT_TO_PAGE), show fit-to-width.
    let fitToChanged = eventToPromise('fit-to-changed', zoomToolbar);
    button.click();
    let result = await fitToChanged;
    assertEquals(FittingType.FIT_TO_PAGE, result.detail);
    assertTrue(button.ironIcon.endsWith(fitWidthIcon));

    // Tap 2: Fire fit-to-changed(FIT_TO_WIDTH), show fit-to-page.
    fitToChanged = eventToPromise('fit-to-changed', zoomToolbar);
    button.click();
    result = await fitToChanged;
    assertEquals(FittingType.FIT_TO_WIDTH, result.detail);
    assertTrue(button.ironIcon.endsWith(fitPageIcon));

    // Tap 3: Fire fit-to-changed(FIT_TO_PAGE) again.
    fitToChanged = eventToPromise('fit-to-changed', zoomToolbar);
    button.click();
    result = await fitToChanged;
    assertEquals(FittingType.FIT_TO_PAGE, result.detail);
    assertTrue(button.ironIcon.endsWith(fitWidthIcon));

    // Do the same as above, but with fitToggleFromHotKey().
    fitToChanged = eventToPromise('fit-to-changed', zoomToolbar);
    zoomToolbar.fitToggleFromHotKey();
    result = await fitToChanged;
    assertEquals(FittingType.FIT_TO_WIDTH, result.detail);
    assertTrue(button.ironIcon.endsWith(fitPageIcon));

    fitToChanged = eventToPromise('fit-to-changed', zoomToolbar);
    zoomToolbar.fitToggleFromHotKey();
    result = await fitToChanged;
    assertEquals(FittingType.FIT_TO_PAGE, result.detail);
    assertTrue(button.ironIcon.endsWith(fitWidthIcon));

    fitToChanged = eventToPromise('fit-to-changed', zoomToolbar);
    zoomToolbar.fitToggleFromHotKey();
    result = await fitToChanged;
    assertEquals(FittingType.FIT_TO_WIDTH, result.detail);
    assertTrue(button.ironIcon.endsWith(fitPageIcon));

    // Tap 4: Fire fit-to-changed(FIT_TO_PAGE) again.
    fitToChanged = eventToPromise('fit-to-changed', zoomToolbar);
    button.click();
    result = await fitToChanged;
    assertEquals(FittingType.FIT_TO_PAGE, result.detail);
    assertTrue(button.ironIcon.endsWith(fitWidthIcon));
  });

  test(assert(pdf_zoom_toolbar_test.TestNames.ForceFitToPage), async () => {
    // Initial: Show fit-to-page.
    assertTrue(button.ironIcon.endsWith(fitPageIcon));

    // Test forceFit(FIT_TO_PAGE) from initial state.
    zoomToolbar.forceFit(FittingType.FIT_TO_PAGE);
    assertTrue(button.ironIcon.endsWith(fitWidthIcon));

    // Tap 1: Fire fit-to-changed(FIT_TO_WIDTH).
    let fitToChanged = eventToPromise('fit-to-changed', zoomToolbar);
    button.click();
    let result = await fitToChanged;
    assertEquals(FittingType.FIT_TO_WIDTH, result.detail);
    assertTrue(button.ironIcon.endsWith(fitPageIcon));

    // Test forceFit(FIT_TO_PAGE) from fit-to-width mode.
    zoomToolbar.forceFit(FittingType.FIT_TO_PAGE);
    assertTrue(button.ironIcon.endsWith(fitWidthIcon));

    // Test forceFit(FIT_TO_PAGE) when already in fit-to-page mode.
    zoomToolbar.forceFit(FittingType.FIT_TO_PAGE);
    assertTrue(button.ironIcon.endsWith(fitWidthIcon));

    // Tap 2: Fire fit-to-changed(FIT_TO_WIDTH).
    fitToChanged = eventToPromise('fit-to-changed', zoomToolbar);
    button.click();
    result = await fitToChanged;
    assertEquals(FittingType.FIT_TO_WIDTH, result.detail);
    assertTrue(button.ironIcon.endsWith(fitPageIcon));
  });
});
