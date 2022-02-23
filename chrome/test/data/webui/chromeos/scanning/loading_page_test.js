// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://scanning/loading_page.js';

import {AppState} from 'chrome://scanning/scanning_app_types.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible} from '../../test_util.js';

export function loadingPageTest() {
  const scanningSrcBase = 'chrome://scanning/';

  /** @type {?LoadingPageElement} */
  let loadingPage = null;

  /**
   * @suppress {visibility}
   * @param {boolean} enabled
   */
  function setIsDarkModeEnabled_(enabled) {
    assertTrue(!!loadingPage);
    loadingPage.isDarkModeEnabled_ = enabled;
  }

  setup(() => {
    loadingPage = /** @type {!LoadingPageElement} */ (
        document.createElement('loading-page'));
    assertTrue(!!loadingPage);
    loadingPage.appState = AppState.GETTING_SCANNERS;
    document.body.appendChild(loadingPage);
  });

  teardown(() => {
    loadingPage.remove();
    loadingPage = null;
  });

  // Verify the loading page, then the no scanners page is shown when no
  // scanners are available.
  test('noScanners', () => {
    assertTrue(
        isVisible(/** @type {!HTMLElement} */ (loadingPage.$$('#loadingDiv'))));
    assertFalse(isVisible(
        /** @type {!HTMLElement} */ (loadingPage.$$('#noScannersDiv'))));

    loadingPage.appState = AppState.NO_SCANNERS;
    assertFalse(
        isVisible(/** @type {!HTMLElement} */ (loadingPage.$$('#loadingDiv'))));
    assertTrue(isVisible(
        /** @type {!HTMLElement} */ (loadingPage.$$('#noScannersDiv'))));
  });

  // Verify clicking the retry button on the no scanners page fires the
  // 'retry-click' event.
  test('retryClick', () => {
    loadingPage.appState = AppState.NO_SCANNERS;

    let retryEventFired = false;
    loadingPage.addEventListener('retry-click', function() {
      retryEventFired = true;
    });

    loadingPage.$$('#retryButton').click();
    assertTrue(retryEventFired);
  });

  // Verify clicking the learn more button on the no scanners page fires the
  // 'learn-more-click' event.
  test('learnMoreClick', () => {
    loadingPage.appState = AppState.NO_SCANNERS;

    let learnMoreEventFired = false;
    loadingPage.addEventListener('learn-more-click', function() {
      learnMoreEventFired = true;
    });

    loadingPage.$$('#learnMoreButton').click();
    assertTrue(learnMoreEventFired);
  });

  // Verify correct 'no scanners' svg displayed when page is in dark mode.
  test('noScannersSvgSetByColorScheme', async () => {
    const lightModeSvg = `${scanningSrcBase}svg/no_scanners.svg`;
    const darkModeSvg = `${scanningSrcBase}svg/no_scanners_dark.svg`;
    const getNoScannersSvg = () => (/** @type {!HTMLImageElement} */ (
        loadingPage.$$('#noScannersDiv img')));

    // Setup UI to display no scanners div.
    loadingPage.appState = AppState.NO_SCANNERS;
    await flushTasks();
    assertEquals(getNoScannersSvg().src, lightModeSvg);

    // Mock media query state for dark mode.
    setIsDarkModeEnabled_(true);
    await flushTasks();
    assertEquals(getNoScannersSvg().src, darkModeSvg);
  });

  // Verify correct 'loading scanners' svg displayed when page is in dark mode.
  test('scanLoadingSvgSetByColorScheme', async () => {
    const lightModeSvg = `${scanningSrcBase}svg/scanners_loading.svg`;
    const darkModeSvg = `${scanningSrcBase}svg/scanners_loading_dark.svg`;
    const getLoadingSvg = () =>
        (/** @type {!HTMLImageElement} */ (loadingPage.$$('#loadingDiv img')));

    // Setup UI to display no scanners div.
    loadingPage.appState = AppState.NO_SCANNERS;
    await flushTasks();
    assertEquals(getLoadingSvg().src, lightModeSvg);

    // Mock media query state for dark mode.
    setIsDarkModeEnabled_(true);
    await flushTasks();
    assertEquals(getLoadingSvg().src, darkModeSvg);
  });
}
