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
   * Type alias for SVGUseElement.
   * @typedef {{href: {baseVal: string}}}
   */
  let SVGUseElement;

  /**
   * @param {boolean} enabled
   * @return {!Promise}
   */
  function setFakePrefersColorSchemeDark(enabled) {
    assertTrue(!!loadingPage);
    fakePrefersColorSchemeDarkMediaQuery.matches = enabled;

    return flushTasks();
  }

  /**
   * @param {boolean} enabled
   * @returns {!Promise}
   */
  function setJellyEnabled(enabled) {
    assertTrue(!!loadingPage);
    loadingPage.setIsJellyEnabledForTesting(enabled);

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

  // TODO(b/276493795): After the Jelly experiment is launched, remove test.
  // Verify correct 'no scanners' svg displayed when page is in dark mode.
  test('noScannersSvgSetByColorScheme', async () => {
    await setJellyEnabled(false);
    const lightModeSvg = `${scanningSrcBase}svg/no_scanners.svg`;
    const darkModeSvg = `${scanningSrcBase}svg/no_scanners_dark.svg`;
    const getNoScannersSvg = () => (/** @type {!HTMLImageElement} */ (
        loadingPage.$$('#noScannersDiv img')));

    // Setup UI to display no scanners div.
    loadingPage.appState = AppState.NO_SCANNERS;
    await setFakePrefersColorSchemeDark(false);
    assertEquals(lightModeSvg, getNoScannersSvg().src);

    // Mock media query state for dark mode.
    await setFakePrefersColorSchemeDark(true);
    assertEquals(darkModeSvg, getNoScannersSvg().src);
  });

  // Verify "no scanners" dynamic SVG use when dynamic colors enabled.
  test('jellyColors_NoScannersSvg', async () => {
    await setJellyEnabled(true);
    const dynamicSvg = `svg/illo_no_scanner.svg#illo_no_scanner`;
    const getNoScannersSvgValue = () =>
        (/** @type {!SVGUseElement} */ (
             loadingPage.shadowRoot.querySelector('#noScannersDiv > svg > use'))
             .href.baseVal);

    // Setup UI to display no scanners div.
    loadingPage.appState = AppState.NO_SCANNERS;
    await setFakePrefersColorSchemeDark(false);
    assertEquals(dynamicSvg, getNoScannersSvgValue());

    // Mock media query state for dark mode.
    await setFakePrefersColorSchemeDark(true);
    assertEquals(dynamicSvg, getNoScannersSvgValue());
  });

  // TODO(b/276493795): After the Jelly experiment is launched, remove test.
  // Verify correct 'loading scanners' svg displayed when page is in dark mode.
  test('scanLoadingSvgSetByColorScheme', async () => {
    await setJellyEnabled(false);
    const lightModeSvg = `${scanningSrcBase}svg/scanners_loading.svg`;
    const darkModeSvg = `${scanningSrcBase}svg/scanners_loading_dark.svg`;
    const getLoadingSvg = () =>
        (/** @type {!HTMLImageElement} */ (loadingPage.$$('#loadingDiv img')));

    // Setup UI to display no scanners div.
    loadingPage.appState = AppState.NO_SCANNERS;
    await setFakePrefersColorSchemeDark(false);
    assertEquals(lightModeSvg, getLoadingSvg().src);

    // Mock media query state for dark mode.
    await setFakePrefersColorSchemeDark(true);
    assertEquals(darkModeSvg, getLoadingSvg().src);
  });

  // Verify "loading scanners" dynamic SVG use when dynamic colors enabled.
  test('jellyColors_LoadingScannersSvg', async () => {
    await setJellyEnabled(true);
    const dynamicSvg = `svg/illo_loading_scanner.svg#illo_loading_scanner`;

    const getLoadingScannersSvgValue = () =>
        (/** @type {!SVGUseElement} */ (
             loadingPage.shadowRoot.querySelector('#loadingDiv > svg > use'))
             .href.baseVal);

    // Setup UI to display no scanners div.
    loadingPage.appState = AppState.NO_SCANNERS;
    await setFakePrefersColorSchemeDark(false);
    assertEquals(dynamicSvg, getLoadingScannersSvgValue());

    // Mock media query state for dark mode.
    await setFakePrefersColorSchemeDark(true);
    assertEquals(dynamicSvg, getLoadingScannersSvgValue());
  });
});
