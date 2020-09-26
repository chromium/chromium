// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {eventToPromise} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/_test_resources/webui/test_util.m.js';
import {ViewerThumbnailBarElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/elements/viewer-thumbnail-bar.js';
import {ViewerThumbnailElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/elements/viewer-thumbnail.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {testAsync} from './test_util.js';

/** @return {!ViewerThumbnailBarElement} */
function createThumbnailBar() {
  document.body.innerHTML = '';
  const thumbnailBar = /** @type {!ViewerThumbnailBarElement} */ (
      document.createElement('viewer-thumbnail-bar'));
  document.body.appendChild(thumbnailBar);
  return thumbnailBar;
}

// Unit tests for the viewer-thumbnail-bar element.
const tests = [
  // Test that the thumbnail bar has the correct number of thumbnails and
  // correspond to the right pages.
  function testThumbnails() {
    const testDocLength = 10;
    const thumbnailBar = createThumbnailBar();
    thumbnailBar.docLength = testDocLength;

    flush();

    // Test that the correct number of viewer-thumbnail elements was created.
    const thumbnails =
        /** @type {!NodeList<!ViewerThumbnailElement>} */ (
            thumbnailBar.shadowRoot.querySelectorAll('viewer-thumbnail'));
    chrome.test.assertEq(testDocLength, thumbnails.length);

    testAsync(async () => {
      /**
       * @param {!ViewerThumbnailElement} thumbnail
       * @param {number} expectedPageIndex
       * @return {!Promise}
       */
      function testNavigateThumbnail(thumbnail, expectedPageIndex) {
        const whenChanged = eventToPromise('change-page', thumbnailBar);
        thumbnail.shadowRoot.querySelector('#thumbnail').click();
        return whenChanged.then(e => {
          chrome.test.assertEq(expectedPageIndex, e.detail.page);
          chrome.test.assertEq('thumbnail', e.detail.origin);
        });
      }

      // Test that each thumbnail has the correct page number and navigates to
      // the corresponding page.
      for (let i = 0; i < thumbnails.length; i++) {
        const thumbnail = thumbnails[i];
        chrome.test.assertEq(i + 1, thumbnail.pageNumber);
        await testNavigateThumbnail(thumbnail, i);
      }
    });
  },
  function testTriggerPaint() {
    // Create a viewer-thumbnail element to get the standard height.
    document.body.innerHTML = '';
    const sizerThumbnail = document.createElement('viewer-thumbnail');
    document.body.appendChild(sizerThumbnail);
    // Add 24 to cover padding between thumbnails.
    const thumbnailBarHeight = sizerThumbnail.offsetHeight + 24;

    // Clear HTML for just the thumbnail bar.
    const testDocLength = 4;
    const thumbnailBar = createThumbnailBar();
    thumbnailBar.docLength = testDocLength;

    // Set the height to one thumbnail. One thumbnail should be visible and
    // another should be hidden by intersecting the observer.
    thumbnailBar.style.height = `${thumbnailBarHeight}px`;
    thumbnailBar.style.display = 'block';

    flush();

    const thumbnails =
        /** @type {!NodeList<!ViewerThumbnailElement>} */ (
            thumbnailBar.shadowRoot.querySelectorAll('viewer-thumbnail'));

    /**
     * @param {!ViewerThumbnailElement} thumbnail
     * @return {!Promise}
     */
    function paintThumbnailToPromise(thumbnail) {
      return new Promise(resolve => {
        const eventType = 'paint-thumbnail';
        thumbnailBar.addEventListener(eventType, function f(e) {
          if (e.detail === thumbnail) {
            thumbnailBar.removeEventListener(eventType, f);
            resolve(e);
          }
        });
      });
    }

    const whenRequestedPaintingFirst = [
      paintThumbnailToPromise(thumbnails[0]),
      paintThumbnailToPromise(thumbnails[1]),
    ];

    testAsync(async () => {
      await Promise.all(whenRequestedPaintingFirst);

      // Only two thumbnails should be pending.
      for (let i = 0; i < thumbnails.length; i++) {
        chrome.test.assertEq(i < 2, thumbnails[i].hasAttribute('pending'));
      }

      // Test that scrolling to the bottom triggers 'paint-thumbnail' events for
      // the remaining thumbnails.
      const whenRequestedPaintingLast = [
        paintThumbnailToPromise(thumbnails[2]),
        paintThumbnailToPromise(thumbnails[3]),
      ];

      thumbnailBar.shadowRoot.querySelector('#thumbnails').scrollTop =
          2 * thumbnailBarHeight;
      await Promise.all(whenRequestedPaintingLast);

      for (const thumbnail of thumbnails) {
        chrome.test.assertTrue(thumbnail.hasAttribute('pending'));
      }
    });
  }
];

chrome.test.runTests(tests);
