// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens/side_panel/side_panel_app.js';

import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';
import type {LensSidePanelAppElement} from 'chrome-untrusted://lens/side_panel/side_panel_app.js';
import {SidePanelBrowserProxyImpl} from 'chrome-untrusted://lens/side_panel/side_panel_browser_proxy.js';
import {assertEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';

import {TestLensSidePanelBrowserProxy} from './test_side_panel_browser_proxy.js';

// The url query parameter keys for the viewport size.
const VIEWPORT_HEIGHT_KEY = 'bih';
const VIEWPORT_WIDTH_KEY = 'biw';

suite('SidePanelResultsFrame', () => {
  let testBrowserProxy: TestLensSidePanelBrowserProxy;
  let lensSidePanelElement: LensSidePanelAppElement;

  setup(() => {
    testBrowserProxy = new TestLensSidePanelBrowserProxy();
    SidePanelBrowserProxyImpl.setInstance(testBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    lensSidePanelElement = document.createElement('lens-side-panel-app');
    document.body.appendChild(lensSidePanelElement);
  });

  test('LoadResultsInFrame', async () => {
    const url: Url = {url: 'https://www.google.com/'};
    testBrowserProxy.page.loadResultsInFrame(url);
    await flushTasks();
    const loadedUrl = new URL(lensSidePanelElement.$.results.src);
    assertTrue(loadedUrl.searchParams.has(VIEWPORT_HEIGHT_KEY));
    assertTrue(loadedUrl.searchParams.has(VIEWPORT_WIDTH_KEY));
    loadedUrl.searchParams.delete(VIEWPORT_HEIGHT_KEY);
    loadedUrl.searchParams.delete(VIEWPORT_WIDTH_KEY);
    assertEquals(loadedUrl.href, url.url);
  });

  test('LoadingStateChangeShowsAndHideResults', async () => {
    // Since the two elements are completely overlapping, the element with the
    // larger z-index is the one that is visible.
    let loadingZIndex = parseInt(
        window.getComputedStyle(lensSidePanelElement.$.ghostLoader).zIndex);
    let resultsZIndex = parseInt(
        window.getComputedStyle(lensSidePanelElement.$.results).zIndex);
    assertTrue(loadingZIndex > resultsZIndex);

    testBrowserProxy.page.setIsLoadingResults(false);
    await waitAfterNextRender(lensSidePanelElement);

    loadingZIndex = parseInt(
        window.getComputedStyle(lensSidePanelElement.$.ghostLoader).zIndex);
    resultsZIndex = parseInt(
        window.getComputedStyle(lensSidePanelElement.$.results).zIndex);
    assertTrue(loadingZIndex < resultsZIndex);

    testBrowserProxy.page.setIsLoadingResults(true);
    await waitAfterNextRender(lensSidePanelElement);

    loadingZIndex = parseInt(
        window.getComputedStyle(lensSidePanelElement.$.ghostLoader).zIndex);
    resultsZIndex = parseInt(
        window.getComputedStyle(lensSidePanelElement.$.results).zIndex);
    assertTrue(loadingZIndex > resultsZIndex);
  });
});
