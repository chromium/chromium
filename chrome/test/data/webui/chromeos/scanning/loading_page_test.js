// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://scanning/loading_page.js';

import {AppState} from 'chrome://scanning/scanning_app_types.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {isVisible} from '../../test_util.m.js';

export function loadingPageTest() {
  /** @type {?LoadingPageElement} */
  let loadingPage = null;

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

  test('retryClick', () => {
    loadingPage.appState = AppState.NO_SCANNERS;

    let retryEventFired = false;
    loadingPage.addEventListener('retry-click', function() {
      retryEventFired = true;
    });

    loadingPage.$$('#retryButton').click();
    assertTrue(retryEventFired);
  });

  test('learnMoreClick', () => {
    loadingPage.appState = AppState.NO_SCANNERS;

    let learnMoreEventFired = false;
    loadingPage.addEventListener('learn-more-click', function() {
      learnMoreEventFired = true;
    });

    loadingPage.$$('#learnMoreButton').click();
    assertTrue(learnMoreEventFired);
  });
}
