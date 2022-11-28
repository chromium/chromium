// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning_mojom_imports.js';
import 'chrome://scanning/loading_page.js';

import {AppState} from 'chrome://scanning/scanning_app_types.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockController} from 'chrome://webui-test/chromeos/mock_controller.m.js';
import {isVisible} from 'chrome://webui-test/chromeos/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {FakeMediaQueryList} from './scanning_app_test_utils.js';

suite('loadingPageTest', function() {
  const scanningSrcBase = 'chrome://scanning/';

  /** @type {?LoadingPageElement} */
  let loadingPage = null;

  /** @type {{createFunctionMock: Function, reset: Function}} */
  let mockController;

  /** @type {?FakeMediaQueryList} */
  let fakePrefersColorSchemeDarkMediaQuery = null;

  /**
   * @param {boolean} enabled
   * @return {!Promise}
   */
  function setFakePrefersColorSchemeDark(enabled) {
    assertTrue(!!loadingPage);
    fakePrefersColorSchemeDarkMediaQuery.matches = enabled;

    return flushTasks();
  }


  setup(() => {
    loadingPage = /** @type {!LoadingPageElement} */ (
        document.createElement('loading-page'));
    assertTrue(!!loadingPage);
    loadingPage.appState = AppState.GETTING_SCANNERS;

    // Setup mock for matchMedia.
    mockController = new MockController();
    const mockMatchMedia =
        mockController.createFunctionMock(window, 'matchMedia');
    fakePrefersColorSchemeDarkMediaQuery =
        new FakeMediaQueryList('(prefers-color-scheme: dark)');
    mockMatchMedia.returnValue = fakePrefersColorSchemeDarkMediaQuery;

    document.body.appendChild(loadingPage);
  });

  teardown(() => {
    mockController.reset();
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
    await setFakePrefersColorSchemeDark(false);
    assertEquals(getNoScannersSvg().src, lightModeSvg);

    // Mock media query state for dark mode.
    await setFakePrefersColorSchemeDark(true);
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
    await setFakePrefersColorSchemeDark(false);
    assertEquals(getLoadingSvg().src, lightModeSvg);

    // Mock media query state for dark mode.
    await setFakePrefersColorSchemeDark(true);
    assertEquals(getLoadingSvg().src, darkModeSvg);
  });
});
