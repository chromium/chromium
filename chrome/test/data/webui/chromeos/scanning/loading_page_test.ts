// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning_mojom_imports.js';
import 'chrome://scanning/loading_page.js';

import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {LoadingPageElement} from 'chrome://scanning/loading_page.js';
import {AppState} from 'chrome://scanning/scanning_app_types.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {isVisible, eventToPromise} from 'chrome://webui-test/chromeos/test_util.js';
import {MockController} from 'chrome://webui-test/mock_controller.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {FakeMediaQueryList} from './scanning_app_test_utils.js';

suite('loadingPageTest', function() {
  const scanningSrcBase = 'chrome://scanning/';

  let loadingPage: LoadingPageElement|null = null;

  let mockController: MockController;

  let fakePrefersColorSchemeDarkMediaQuery: FakeMediaQueryList|null = null;

  function setFakePrefersColorSchemeDark(enabled: boolean): Promise<void> {
    assert(loadingPage);
    fakePrefersColorSchemeDarkMediaQuery!.matches = enabled;

    return flushTasks();
  }

  function setJellyEnabled(enabled: boolean): Promise<void> {
    assert(loadingPage);
    loadingPage.setIsJellyEnabledForTesting(enabled);

    return flushTasks();
  }


  setup(() => {
    loadingPage = document.createElement('loading-page');
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
    loadingPage?.remove();
    loadingPage = null;
  });

  // Verify the loading page, then the no scanners page is shown when no
  // scanners are available.
  test('noScanners', () => {
    assert(loadingPage);
    assertTrue(isVisible(
        strictQuery('#loadingDiv', loadingPage.shadowRoot, HTMLElement)));
    assertFalse(isVisible(
        strictQuery('#noScannersDiv', loadingPage.shadowRoot, HTMLElement)));

    loadingPage.appState = AppState.NO_SCANNERS;
    assertFalse(isVisible(
        strictQuery('#loadingDiv', loadingPage.shadowRoot, HTMLElement)));
    assertTrue(isVisible(
        strictQuery('#noScannersDiv', loadingPage.shadowRoot, HTMLElement)));
  });

  // Verify clicking the retry button on the no scanners page fires the
  // 'retry-click' event.
  test('retryClick', () => {
    assert(loadingPage);
    loadingPage.appState = AppState.NO_SCANNERS;

    let retryEventFired = false;
    loadingPage.addEventListener('retry-click', function() {
      retryEventFired = true;
    });

    strictQuery('#retryButton', loadingPage.shadowRoot, HTMLElement).click();
    assertTrue(retryEventFired);
  });

  // Verify clicking the learn more button on the no scanners page fires the
  // 'learn-more-click' event.
  test('learnMoreClick', async () => {
    assert(loadingPage);
    loadingPage.appState = AppState.NO_SCANNERS;

    let learnMoreEventFired = false;
    loadingPage.addEventListener('learn-more-click', function() {
      learnMoreEventFired = true;
    });
    const learnMoreEvent = eventToPromise('learn-more-click', loadingPage);
    strictQuery('#learnMoreButton', loadingPage.shadowRoot, HTMLElement)
        .click();
    await learnMoreEvent;
    assertTrue(learnMoreEventFired);
  });

  // TODO(b/276493795): After the Jelly experiment is launched, remove test.
  // Verify correct 'no scanners' svg displayed when page is in dark mode.
  test('noScannersSvgSetByColorScheme', async () => {
    assert(loadingPage);
    await setJellyEnabled(false);
    const lightModeSvg = `${scanningSrcBase}svg/no_scanners.svg`;
    const darkModeSvg = `${scanningSrcBase}svg/no_scanners_dark.svg`;
    const getNoScannersVisual = (): HTMLImageElement => strictQuery(
        '#noScannersDiv img', loadingPage!.shadowRoot, HTMLImageElement);

    // Setup UI to display no scanners div.
    loadingPage.appState = AppState.NO_SCANNERS;
    await setFakePrefersColorSchemeDark(false);
    assertEquals(lightModeSvg, getNoScannersVisual().src);

    // Mock media query state for dark mode.
    await setFakePrefersColorSchemeDark(true);
    assertEquals(darkModeSvg, getNoScannersVisual().src);
  });

  // Verify "no scanners" dynamic SVG use when dynamic colors enabled.
  test('jellyColors_NoScannersSvg', async () => {
    assert(loadingPage);
    await setJellyEnabled(true);
    const dynamicSvg = `svg/illo_no_scanner.svg#illo_no_scanner`;
    const getNoScannersVisual = (): SVGUseElement => strictQuery(
        '#noScannersDiv > svg > use', loadingPage!.shadowRoot, SVGUseElement);

    // Setup UI to display no scanners div.
    loadingPage.appState = AppState.NO_SCANNERS;
    await setFakePrefersColorSchemeDark(false);
    assertEquals(dynamicSvg, getNoScannersVisual().href.baseVal);

    // Mock media query state for dark mode.
    await setFakePrefersColorSchemeDark(true);
    assertEquals(dynamicSvg, getNoScannersVisual().href.baseVal);
  });

  // TODO(b/276493795): After the Jelly experiment is launched, remove test.
  // Verify correct 'loading scanners' svg displayed when page is in dark mode.
  test('scanLoadingSvgSetByColorScheme', async () => {
    assert(loadingPage);
    await setJellyEnabled(false);
    const lightModeSvg = `${scanningSrcBase}svg/scanners_loading.svg`;
    const darkModeSvg = `${scanningSrcBase}svg/scanners_loading_dark.svg`;
    const getLoadingVisual = (): HTMLImageElement => strictQuery(
        '#loadingDiv img', loadingPage!.shadowRoot, HTMLImageElement);

    // Setup UI to display no scanners div.
    loadingPage.appState = AppState.NO_SCANNERS;
    await setFakePrefersColorSchemeDark(false);
    assertEquals(lightModeSvg, getLoadingVisual().src);

    // Mock media query state for dark mode.
    await setFakePrefersColorSchemeDark(true);
    assertEquals(darkModeSvg, getLoadingVisual().src);
  });

  // Verify "loading scanners" dynamic SVG use when dynamic colors enabled.
  test('jellyColors_LoadingScannersSvg', async () => {
    assert(loadingPage);
    await setJellyEnabled(true);
    const dynamicSvg = `svg/illo_loading_scanner.svg#illo_loading_scanner`;

    const getLoadingVisual = (): SVGUseElement => strictQuery(
        '#loadingDiv > svg > use', loadingPage!.shadowRoot, SVGUseElement);

    // Setup UI to display no scanners div.
    loadingPage.appState = AppState.NO_SCANNERS;
    await setFakePrefersColorSchemeDark(false);
    assertEquals(dynamicSvg, getLoadingVisual().href.baseVal);

    // Mock media query state for dark mode.
    await setFakePrefersColorSchemeDark(true);
    assertEquals(dynamicSvg, getLoadingVisual().href.baseVal);
  });
});
