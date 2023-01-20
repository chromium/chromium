// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrIconButtonElement, FittingType, ViewerZoomButtonElement, ViewerZoomToolbarElement} from 'chrome://print/pdf/pdf_print_wrapper.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

const pdf_zoom_toolbar_test = {
  suiteName: 'PdfZoomToolbarTest',
  TestNames: {
    Toggle: 'toggle',
    ForceFitToPage: 'force fit to page',
  },
};

Object.assign(window, {pdf_zoom_toolbar_test: pdf_zoom_toolbar_test});

suite(pdf_zoom_toolbar_test.suiteName, function() {
  let zoomToolbar: ViewerZoomToolbarElement;

  let fitButton: ViewerZoomButtonElement;

  let button: CrIconButtonElement;

  const fitWidthIcon: string = 'fullscreen';
  const fitPageIcon: string = 'fullscreen-exit';

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    zoomToolbar = document.createElement('viewer-zoom-toolbar');
    document.body.appendChild(zoomToolbar);

    fitButton = zoomToolbar.$.fitButton;
    button = fitButton.shadowRoot!.querySelector('cr-icon-button')!;
  });

  /**
   * Test that the zoom toolbar toggles between showing the fit-to-page and
   * fit-to-width buttons.
   */
  test(pdf_zoom_toolbar_test.TestNames.Toggle, async () => {
    // Initial: Show fit-to-page.
    assertTrue(button.ironIcon!.endsWith(fitPageIcon));

    // Tap 1: Fire fit-to-changed(FIT_TO_PAGE), show fit-to-width.
    let fitToChanged = eventToPromise('fit-to-changed', zoomToolbar);
    button.click();
    let result = await fitToChanged;
    assertEquals(FittingType.FIT_TO_PAGE, result.detail);
    assertTrue(button.ironIcon!.endsWith(fitWidthIcon));

    // Tap 2: Fire fit-to-changed(FIT_TO_WIDTH), show fit-to-page.
    fitToChanged = eventToPromise('fit-to-changed', zoomToolbar);
    button.click();
    result = await fitToChanged;
    assertEquals(FittingType.FIT_TO_WIDTH, result.detail);
    assertTrue(button.ironIcon!.endsWith(fitPageIcon));

    // Tap 3: Fire fit-to-changed(FIT_TO_PAGE) again.
    fitToChanged = eventToPromise('fit-to-changed', zoomToolbar);
    button.click();
    result = await fitToChanged;
    assertEquals(FittingType.FIT_TO_PAGE, result.detail);
    assertTrue(button.ironIcon!.endsWith(fitWidthIcon));

    // Do the same as above, but with fitToggleFromHotKey().
    fitToChanged = eventToPromise('fit-to-changed', zoomToolbar);
    zoomToolbar.fitToggleFromHotKey();
    result = await fitToChanged;
    assertEquals(FittingType.FIT_TO_WIDTH, result.detail);
    assertTrue(button.ironIcon!.endsWith(fitPageIcon));

    fitToChanged = eventToPromise('fit-to-changed', zoomToolbar);
    zoomToolbar.fitToggleFromHotKey();
    result = await fitToChanged;
    assertEquals(FittingType.FIT_TO_PAGE, result.detail);
    assertTrue(button.ironIcon!.endsWith(fitWidthIcon));

    fitToChanged = eventToPromise('fit-to-changed', zoomToolbar);
    zoomToolbar.fitToggleFromHotKey();
    result = await fitToChanged;
    assertEquals(FittingType.FIT_TO_WIDTH, result.detail);
    assertTrue(button.ironIcon!.endsWith(fitPageIcon));

    // Tap 4: Fire fit-to-changed(FIT_TO_PAGE) again.
    fitToChanged = eventToPromise('fit-to-changed', zoomToolbar);
    button.click();
    result = await fitToChanged;
    assertEquals(FittingType.FIT_TO_PAGE, result.detail);
    assertTrue(button.ironIcon!.endsWith(fitWidthIcon));
  });

  test(pdf_zoom_toolbar_test.TestNames.ForceFitToPage, async () => {
    // Initial: Show fit-to-page.
    assertTrue(button.ironIcon!.endsWith(fitPageIcon));

    // Test forceFit(FIT_TO_PAGE) from initial state.
    zoomToolbar.forceFit(FittingType.FIT_TO_PAGE);
    assertTrue(button.ironIcon!.endsWith(fitWidthIcon));

    // Tap 1: Fire fit-to-changed(FIT_TO_WIDTH).
    let fitToChanged = eventToPromise('fit-to-changed', zoomToolbar);
    button.click();
    let result = await fitToChanged;
    assertEquals(FittingType.FIT_TO_WIDTH, result.detail);
    assertTrue(button.ironIcon!.endsWith(fitPageIcon));

    // Test forceFit(FIT_TO_PAGE) from fit-to-width mode.
    zoomToolbar.forceFit(FittingType.FIT_TO_PAGE);
    assertTrue(button.ironIcon!.endsWith(fitWidthIcon));

    // Test forceFit(FIT_TO_PAGE) when already in fit-to-page mode.
    zoomToolbar.forceFit(FittingType.FIT_TO_PAGE);
    assertTrue(button.ironIcon!.endsWith(fitWidthIcon));

    // Tap 2: Fire fit-to-changed(FIT_TO_WIDTH).
    fitToChanged = eventToPromise('fit-to-changed', zoomToolbar);
    button.click();
    result = await fitToChanged;
    assertEquals(FittingType.FIT_TO_WIDTH, result.detail);
    assertTrue(button.ironIcon!.endsWith(fitPageIcon));
  });
});
