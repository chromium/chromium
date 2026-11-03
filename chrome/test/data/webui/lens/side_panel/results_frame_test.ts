// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens/side_panel/side_panel_app.js';

import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';
import type {LensSidePanelPageRemote} from 'chrome-untrusted://lens/lens_side_panel.mojom-webui.js';
import {browserProxyFactory} from 'chrome-untrusted://lens/lens_side_panel.mojom-webui.js';
import type {LensSidePanelAppElement} from 'chrome-untrusted://lens/side_panel/side_panel_app.js';
import {assertEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';

import {TestLensSidePanelPageHandler} from './test_side_panel_browser_proxy.js';

// The url query parameter keys for the viewport size.
const VIEWPORT_HEIGHT_KEY = 'bih';
const VIEWPORT_WIDTH_KEY = 'biw';

suite('SidePanelResultsFrame', () => {
  let handler: TestLensSidePanelPageHandler;
  let pageRemote: LensSidePanelPageRemote;
  let lensSidePanelElement: LensSidePanelAppElement;

  setup(async () => {
    handler = new TestLensSidePanelPageHandler();
    const {instance, remote} = browserProxyFactory.createForTest(handler);
    browserProxyFactory.setInstance(instance);
    pageRemote = remote;

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    lensSidePanelElement = document.createElement('lens-side-panel-app');
    // Side panel should call getIsContextualSearchbox on construction.
    await handler.whenCalled('getIsContextualSearchbox');
    document.body.appendChild(lensSidePanelElement);
  });

  test('LoadResultsInFrame', async () => {
    const url: Url = 'https://www.google.com/';
    pageRemote.loadResultsInFrame(url);
    await flushTasks();
    const loadedUrl = new URL(lensSidePanelElement.$.results.src);
    assertTrue(loadedUrl.searchParams.has(VIEWPORT_HEIGHT_KEY));
    assertTrue(loadedUrl.searchParams.has(VIEWPORT_WIDTH_KEY));
    loadedUrl.searchParams.delete(VIEWPORT_HEIGHT_KEY);
    loadedUrl.searchParams.delete(VIEWPORT_WIDTH_KEY);
    assertEquals(loadedUrl.href, url);
  });

  test('LoadingStateChangeShowsAndHideResults', async () => {
    // Since the two elements are completely overlapping, the element with the
    // larger z-index is the one that is visible.
    let loadingZIndex = parseInt(
        window.getComputedStyle(lensSidePanelElement.$.ghostLoader).zIndex);
    let resultsZIndex = parseInt(
        window.getComputedStyle(lensSidePanelElement.$.results).zIndex);
    assertTrue(loadingZIndex > resultsZIndex);

    pageRemote.setIsLoadingResults(false);
    await waitAfterNextRender(lensSidePanelElement);

    loadingZIndex = parseInt(
        window.getComputedStyle(lensSidePanelElement.$.ghostLoader).zIndex);
    resultsZIndex = parseInt(
        window.getComputedStyle(lensSidePanelElement.$.results).zIndex);
    assertTrue(loadingZIndex < resultsZIndex);

    pageRemote.setIsLoadingResults(true);
    await waitAfterNextRender(lensSidePanelElement);

    loadingZIndex = parseInt(
        window.getComputedStyle(lensSidePanelElement.$.ghostLoader).zIndex);
    resultsZIndex = parseInt(
        window.getComputedStyle(lensSidePanelElement.$.results).zIndex);
    assertTrue(loadingZIndex > resultsZIndex);
  });
});
