// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {createAutosizeWebview, getWebviewSize, isClose, setGuestContentSize, waitFor, waitForGuestSize, waitForGuestToMatchEmbedderZoom} from './webview_autosize_test_utils.js';
import type {Size} from './webview_autosize_test_utils.js';

declare global {
  interface Window {
    // The URL of a page that evaluates functions posted to it (see
    // chrome/test/data/webui/guest_view_shared/eval_post_message.html).
    webviewUrl: string;
  }
}

// WebUIWebViewBrowserTest.AutoSizeWithZoomedEmbedder runs these with the
// embedder zoomed, where its CSS px and physical pixels differ.
suite('WebviewAutoSizeTest', function() {
  teardown(function() {
    document.body.replaceChildren();
  });

  // The <webview> should be sized to the guest's content, in embedder CSS px.
  test('ReportsSizeInCssPixels', async function() {
    const webview =
        await createAutosizeWebview(window.webviewUrl, 50, 50, 1000, 1000);
    assertEquals('', await waitForGuestToMatchEmbedderZoom(webview));
    await setGuestContentSize(webview, 300, 200);

    const expected: Size = [300, 200];
    await waitFor(() => isClose(expected, getWebviewSize(webview)));
    const actual = getWebviewSize(webview);
    assertTrue(
        isClose(expected, actual),
        `expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
  });

  // The autosize attributes set when the guest is created should limit it to
  // that many embedder CSS px.
  test('AppliesInitialConstraintsInCssPixels', async function() {
    const webview =
        await createAutosizeWebview(window.webviewUrl, 50, 50, 200, 100);
    assertEquals('', await waitForGuestToMatchEmbedderZoom(webview));
    await setGuestContentSize(webview, 300, 300);
    assertEquals('', await waitForGuestSize(webview, 200, 100));
  });

  // Autosize attributes changed after the guest is created should also limit
  // it to that many embedder CSS px.
  test('AppliesUpdatedConstraintsInCssPixels', async function() {
    const webview =
        await createAutosizeWebview(window.webviewUrl, 50, 50, 1000, 1000);
    assertEquals('', await waitForGuestToMatchEmbedderZoom(webview));
    await setGuestContentSize(webview, 300, 300);
    webview.setAttribute('maxwidth', '200');
    webview.setAttribute('maxheight', '100');
    assertEquals('', await waitForGuestSize(webview, 200, 100));
  });
});
