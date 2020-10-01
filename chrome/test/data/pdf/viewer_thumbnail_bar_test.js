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

/** @return {number} */
function getTestThumbnailBarHeight() {
  // Create a viewer-thumbnail element to get the standard height.
  document.body.innerHTML = '';
  const sizerThumbnail = document.createElement('viewer-thumbnail');
  sizerThumbnail.pageNumber = 1;
  document.body.appendChild(sizerThumbnail);
  // Add 24 to cover padding between thumbnails.
  const thumbnailBarHeight = sizerThumbnail.offsetHeight + 24;
  return thumbnailBarHeight;
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
    const thumbnailBarHeight = getTestThumbnailBarHeight();

    // Clear HTML for just the thumbnail bar.
    const testDocLength = 8;
    const thumbnailBar = createThumbnailBar();
    thumbnailBar.docLength = testDocLength;

    // Set the height to one thumbnail. One thumbnail should be visible and
    // another should be hidden by intersecting the observer.
    thumbnailBar.style.height = `${thumbnailBarHeight}px`;
    thumbnailBar.style.display = 'block';

    // Remove any padding from the scroller.
    const scroller = thumbnailBar.shadowRoot.querySelector('#thumbnails');
    scroller.style.padding = '';

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

    testAsync(async () => {
      // Only two thumbnails should be "painted" upon load.
      const whenRequestedPaintingFirst = [
        paintThumbnailToPromise(thumbnails[0]),
        paintThumbnailToPromise(thumbnails[1]),
      ];
      await Promise.all(whenRequestedPaintingFirst);

      chrome.test.assertEq(testDocLength, thumbnails.length);
      for (let i = 0; i < thumbnails.length; i++) {
        chrome.test.assertEq(i < 2, thumbnails[i].isPainted());
      }

      // Test that scrolling to the sixth thumbnail triggers 'paint-thumbnail'
      // for thumbnails 3 through 7. When on the sixth thumbnail, five
      // thumbnails above and one thumbnail below should also be painted because
      // of the 500% top and 100% bottom root margins.
      const whenRequestedPaintingNext = [];
      for (let i = 2; i < 7; i++) {
        whenRequestedPaintingNext.push(paintThumbnailToPromise(thumbnails[i]));
      }
      scroller.scrollTop = 5 * thumbnailBarHeight;
      await Promise.all(whenRequestedPaintingNext);

      // First seven thumbnails should be painted.
      for (let i = 0; i < thumbnails.length; i++) {
        chrome.test.assertEq(i < 7, thumbnails[i].isPainted());
      }

      // Test that scrolling down to the eighth thumbnail will clear the
      // thumbnails outside the root margin, namely the first two. A paint
      // should also be triggered for the eighth thumbnail.
      const whenRequestedPaintingLast = [
        paintThumbnailToPromise(thumbnails[7]),
        eventToPromise('clear-thumbnail-for-testing', thumbnails[0]),
        eventToPromise('clear-thumbnail-for-testing', thumbnails[1]),
      ];
      scroller.scrollTop = 7 * thumbnailBarHeight;
      await Promise.all(whenRequestedPaintingLast);

      // Only first two thumbnails should not be painted.
      for (let i = 0; i < thumbnails.length; i++) {
        chrome.test.assertEq(i > 1, thumbnails[i].isPainted());
      }
    });
  },
];

chrome.test.runTests(tests);
