// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ChangePageAndXyDetail, ChangePageDetail, ChangeZoomDetail, NavigateDetail} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createBookmarksForTest} from './test_util.js';

chrome.test.runTests([
  /**
   * Test that the correct bookmarks were loaded for
   * test-bookmarks-with-zoom.pdf.
   */
  function testHasCorrectBookmarks() {
    const viewer = document.body.querySelector('pdf-viewer')!;
    const bookmarks = viewer.bookmarks;

    // Load all relevant bookmarks.
    chrome.test.assertEq(3, bookmarks.length);
    const firstBookmark = bookmarks[0]!;
    const secondBookmark = bookmarks[1]!;
    const uriBookmark = bookmarks[2]!;
    chrome.test.assertEq(1, firstBookmark.children.length);
    chrome.test.assertEq(0, secondBookmark.children.length);
    const firstNestedBookmark = firstBookmark.children[0]!;

    // Check titles.
    chrome.test.assertEq('First Section', firstBookmark.title);
    chrome.test.assertEq('First Subsection', firstNestedBookmark.title);
    chrome.test.assertEq('Second Section', secondBookmark.title);
    chrome.test.assertEq('URI Bookmark', uriBookmark.title);

    // Check bookmark fields.
    chrome.test.assertEq(0, firstBookmark.page);
    chrome.test.assertEq(133, firstBookmark.x);
    chrome.test.assertEq(667, firstBookmark.y);
    chrome.test.assertEq(1.25, firstBookmark.zoom);
    chrome.test.assertEq(undefined, firstBookmark.uri);

    chrome.test.assertEq(1, firstNestedBookmark.page);
    chrome.test.assertEq(133, firstNestedBookmark.x);
    chrome.test.assertEq(667, firstNestedBookmark.y);
    chrome.test.assertEq(1.5, firstNestedBookmark.zoom);
    chrome.test.assertEq(undefined, firstNestedBookmark.uri);

    chrome.test.assertEq(2, secondBookmark.page);
    chrome.test.assertEq(133, secondBookmark.x);
    chrome.test.assertEq(667, secondBookmark.y);
    chrome.test.assertEq(1.75, secondBookmark.zoom);
    chrome.test.assertEq(undefined, secondBookmark.uri);

    chrome.test.assertEq(undefined, uriBookmark.page);
    chrome.test.assertEq(undefined, uriBookmark.x);
    chrome.test.assertEq(undefined, uriBookmark.y);
    chrome.test.assertEq('http://www.chromium.org', uriBookmark.uri);

    chrome.test.succeed();
  },

  /**
   * Test that a bookmark is followed when clicked in
   * test-bookmarks-with-zoom.pdf.
   */
  async function testFollowBookmark() {
    const viewer = document.body.querySelector('pdf-viewer')!;
    const bookmarkContent = createBookmarksForTest();
    bookmarkContent.bookmarks = viewer.bookmarks;
    document.body.appendChild(bookmarkContent);

    await microtasksFinished();

    const rootBookmarks =
        bookmarkContent.shadowRoot!.querySelectorAll('viewer-bookmark');
    chrome.test.assertEq(3, rootBookmarks.length, 'three root bookmarks');
    const expandButton = rootBookmarks[0]!.$.expand;
    chrome.test.assertEq('false', expandButton.getAttribute('aria-expanded'));
    expandButton.click();

    await microtasksFinished();

    chrome.test.assertEq('true', expandButton.getAttribute('aria-expanded'));
    const subBookmarks =
        rootBookmarks[0]!.shadowRoot!.querySelectorAll('viewer-bookmark');
    chrome.test.assertEq(1, subBookmarks.length, 'one sub bookmark');

    let lastPageChange: number|undefined;
    let lastXChange: number|undefined;
    let lastYChange: number|undefined;
    let lastZoomChange: number|undefined;
    let lastUriNavigation: string|undefined;

    function resetLastChange() {
      lastPageChange = undefined;
      lastXChange = undefined;
      lastYChange = undefined;
      lastZoomChange = undefined;
      lastUriNavigation = undefined;
    }

    bookmarkContent.addEventListener('change-page', function(e) {
      lastPageChange = e.detail.page;
    });
    bookmarkContent.addEventListener('change-zoom', function(e) {
      lastZoomChange = e.detail.zoom;
    });
    bookmarkContent.addEventListener('change-page-and-xy', function(e) {
      lastPageChange = e.detail.page;
      lastXChange = e.detail.x;
      lastYChange = e.detail.y;
    });
    bookmarkContent.addEventListener('navigate', function(e) {
      lastUriNavigation = e.detail.uri;
    });

    type ExpectedEventDetail =
        ChangePageAndXyDetail|ChangePageDetail|ChangeZoomDetail|NavigateDetail;

    async function testTapTarget(
        tapTarget: HTMLElement, expectedDetail: ExpectedEventDetail) {
      resetLastChange();
      tapTarget.click();
      await microtasksFinished();
      chrome.test.assertEq(
          (expectedDetail as ChangePageDetail).page, lastPageChange);
      chrome.test.assertEq(
          (expectedDetail as ChangePageAndXyDetail).x, lastXChange);
      chrome.test.assertEq(
          (expectedDetail as ChangePageAndXyDetail).y, lastYChange);
      chrome.test.assertEq(
          (expectedDetail as ChangeZoomDetail).zoom, lastZoomChange);
      chrome.test.assertEq(
          (expectedDetail as NavigateDetail).uri, lastUriNavigation);
    }

    await testTapTarget(
        rootBookmarks[0]!.$.item, {page: 0, x: 133, y: 667, zoom: 1.25});
    await testTapTarget(
        subBookmarks[0]!.$.item, {page: 1, x: 133, y: 667, zoom: 1.5});
    await testTapTarget(
        rootBookmarks[1]!.$.item, {page: 2, x: 133, y: 667, zoom: 1.75});
    await testTapTarget(
        rootBookmarks[2]!.$.item,
        {uri: 'http://www.chromium.org', newtab: false});

    chrome.test.succeed();
  },
]);
