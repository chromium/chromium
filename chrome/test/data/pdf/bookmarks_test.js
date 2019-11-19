// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PDFScriptingAPI} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_scripting_api.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {createBookmarksForTest} from './test_util.js';

const tests = [
  /**
   * Test that the correct bookmarks were loaded for
   * test-bookmarks-with-zoom.pdf.
   */
  function testHasCorrectBookmarks() {
    const bookmarks = viewer.bookmarks;

    // Load all relevant bookmarks.
    chrome.test.assertEq(3, bookmarks.length);
    const firstBookmark = bookmarks[0];
    const secondBookmark = bookmarks[1];
    const uriBookmark = bookmarks[2];
    chrome.test.assertEq(1, firstBookmark.children.length);
    chrome.test.assertEq(0, secondBookmark.children.length);
    const firstNestedBookmark = firstBookmark.children[0];

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
  function testFollowBookmark() {
    const bookmarkContent = createBookmarksForTest();
    bookmarkContent.bookmarks = viewer.bookmarks;
    document.body.appendChild(bookmarkContent);

    flush();

    const rootBookmarks =
        bookmarkContent.shadowRoot.querySelectorAll('viewer-bookmark');
    chrome.test.assertEq(3, rootBookmarks.length, 'three root bookmarks');
    rootBookmarks[0].$.expand.click();

    flush();

    const subBookmarks =
        rootBookmarks[0].shadowRoot.querySelectorAll('viewer-bookmark');
    chrome.test.assertEq(1, subBookmarks.length, 'one sub bookmark');

    let lastPageChange;
    let lastXChange;
    let lastYChange;
    let lastZoomChange;
    let lastUriNavigation;
    bookmarkContent.addEventListener('change-page', function(e) {
      lastPageChange = e.detail.page;
      lastXChange = undefined;
      lastYChange = undefined;
      lastUriNavigation = undefined;
    });
    bookmarkContent.addEventListener('change-zoom', function(e) {
      lastZoomChange = e.detail.zoom;
    });
    bookmarkContent.addEventListener('change-page-and-xy', function(e) {
      lastPageChange = e.detail.page;
      lastXChange = e.detail.x;
      lastYChange = e.detail.y;
      lastUriNavigation = undefined;
    });
    bookmarkContent.addEventListener('navigate', function(e) {
      lastPageChange = undefined;
      lastXChange = undefined;
      lastYChange = undefined;
      lastUriNavigation = e.detail.uri;
    });

    function testTapTarget(tapTarget, expectedEvent) {
      lastPageChange = undefined;
      lastXChange = undefined;
      lastYChange = undefined;
      lastZoomChange = undefined;
      lastUriNavigation = undefined;
      tapTarget.click();
      chrome.test.assertEq(expectedEvent.page, lastPageChange);
      chrome.test.assertEq(expectedEvent.x, lastXChange);
      chrome.test.assertEq(expectedEvent.y, lastYChange);
      chrome.test.assertEq(expectedEvent.zoom, lastZoomChange);
      chrome.test.assertEq(expectedEvent.uri, lastUriNavigation);
    }

    testTapTarget(
        rootBookmarks[0].$.item, {page: 0, x: 133, y: 667, zoom: 1.25});
    testTapTarget(subBookmarks[0].$.item, {page: 1, x: 133, y: 667, zoom: 1.5});
    testTapTarget(
        rootBookmarks[1].$.item, {page: 2, x: 133, y: 667, zoom: 1.75});
    testTapTarget(rootBookmarks[2].$.item, {uri: 'http://www.chromium.org'});

    chrome.test.succeed();
  }
];

const scriptingAPI = new PDFScriptingAPI(window, window);
scriptingAPI.setLoadCallback(function() {
  chrome.test.runTests(tests);
});
