// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {eventToPromise, whenAttributeIs} from 'chrome://webui-test/test_util.js';

chrome.test.runTests([
  // Verifies that on startup of a multi-page PDF document (linearized.pdf has
  // 538 pages), only an initial batch of thumbnails is queued and loaded
  // rather than all 538 thumbnails at once.
  async function testThumbnailLazyLoading() {
    const viewer = document.body.querySelector('pdf-viewer');
    assert(viewer);
    const sidenav = viewer.shadowRoot.querySelector('viewer-pdf-sidenav');
    assert(sidenav);
    const thumbnailBar =
        sidenav.shadowRoot.querySelector('viewer-thumbnail-bar');
    assert(thumbnailBar);

    // Check if the initial batch of thumbnails has already been painted.
    let painted =
        thumbnailBar.shadowRoot.querySelectorAll('viewer-thumbnail[painted]');
    if (painted.length === 0) {
      await eventToPromise('thumbnails-processed-for-testing', thumbnailBar);
      painted =
          thumbnailBar.shadowRoot.querySelectorAll('viewer-thumbnail[painted]');
    }

    // In practice 8-10 thumbnails are loaded. The window size is 1024x768 and
    // thumbnails are ~190px (with decorations). The lazy loading code also
    // loads 2x the visible thumbnails. 2*768/190 = ~8.
    chrome.test.assertTrue(
        painted.length > 0,
        'Expected at least one thumbnail to be painted, but none were.');
    chrome.test.assertTrue(
        painted.length <= 15,
        `Expected <= 15 thumbnails to be painted, but got ${painted.length}.`);

    chrome.test.succeed();
  },

  // Verifies that scrolling deeper into the document loads subsequent
  // thumbnails.
  async function testThumbnailLoadOnScroll() {
    const viewer = document.body.querySelector('pdf-viewer');
    assert(viewer);
    const sidenav = viewer.shadowRoot.querySelector('viewer-pdf-sidenav');
    assert(sidenav);
    const thumbnailBar =
        sidenav.shadowRoot.querySelector('viewer-thumbnail-bar');
    assert(thumbnailBar);

    // Page 50 should not be painted on initial load.
    const thumbnail50 = thumbnailBar.getThumbnailForPage(50);
    assert(thumbnail50);
    chrome.test.assertFalse(
        thumbnail50.isPainted(),
        'Page 50 should not be painted before scrolling.');

    // Scroll page 50 into view and wait for it to be painted.
    const whenPainted = whenAttributeIs(thumbnail50, 'painted', '');
    thumbnail50.scrollIntoView();
    await whenPainted;

    const painted =
        thumbnailBar.shadowRoot.querySelectorAll('viewer-thumbnail[painted]');
    chrome.test.assertTrue(painted.length > 0);
    chrome.test.assertTrue(
        painted.length <= 75,
        'Expected <= 75 thumbnails painted after scroll, but got ' +
            painted.length);

    chrome.test.succeed();
  },
]);
