// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createBookmarksForTest} from './test_util.js';

/**
 * Standalone unit tests of the PDF Polymer elements.
 */
const tests = [
  /**
   * Test that viewer-page-selector reacts correctly to text entry. The page
   * selector validates that input is an integer, and does not allow navigation
   * past document bounds.
   */
  function testPageSelectorChange() {
    document.body.innerHTML = '';
    const selector = document.createElement('viewer-page-selector')!;
    selector.docLength = 1234;
    document.body.appendChild(selector);

    const input = selector.$.pageSelector;
    // Simulate entering text into `input` and pressing enter.
    function changeInput(newValue: string) {
      input.value = newValue;
      input.dispatchEvent(new CustomEvent('input'));
      input.dispatchEvent(new CustomEvent('change'));
    }

    const navigatedPages: number[] = [];
    selector.addEventListener('change-page', function(e) {
      navigatedPages.push(e.detail.page);
      // A change-page handler is expected to set the pageNo to the new value.
      selector.pageNo = e.detail.page + 1;
    });

    changeInput('1000');
    changeInput('1234');
    changeInput('abcd');
    changeInput('12pp');
    changeInput('3.14');
    changeInput('3000');

    chrome.test.assertEq(4, navigatedPages.length);
    // The event page number is 0-based.
    chrome.test.assertEq(999, navigatedPages[0]);
    chrome.test.assertEq(1233, navigatedPages[1]);
    chrome.test.assertEq(11, navigatedPages[2]);
    // If the user types 3.14, the . will be ignored but 14 will be captured.
    chrome.test.assertEq(313, navigatedPages[3]);

    chrome.test.succeed();
  },

  /**
   * Test that viewer-page-selector changes in response to setting docLength.
   */
  function testPageSelectorDocLength() {
    document.body.innerHTML = '';
    const selector = document.createElement('viewer-page-selector');
    selector.docLength = 1234;
    document.body.appendChild(selector);
    chrome.test.assertEq(
        '1234', selector.shadowRoot!.querySelector('#pagelength')!.textContent);
    chrome.test.assertEq(
        '4', selector.style.getPropertyValue('--page-length-digits'));
    chrome.test.succeed();
  },

  /**
   * Test that viewer-bookmarks-content creates a bookmark tree with the correct
   * structure and behaviour.
   */
  async function testBookmarkStructure() {
    document.body.innerHTML = '';
    const bookmarkContent = createBookmarksForTest();
    bookmarkContent.bookmarks = [{
      title: 'Test 1',
      page: 1,
      children: [
        {title: 'Test 1a', page: 2, children: []},
        {title: 'Test 1b', page: 3, children: []},
      ],
    }];
    document.body.appendChild(bookmarkContent);

    // Wait for templates to render.
    await microtasksFinished();

    const rootBookmarks =
        bookmarkContent.shadowRoot!.querySelectorAll('viewer-bookmark');
    chrome.test.assertEq(1, rootBookmarks.length, 'one root bookmark');
    const rootBookmark = rootBookmarks[0]!;
    rootBookmark.$.expand.click();

    await microtasksFinished();

    const subBookmarks =
        rootBookmark.shadowRoot!.querySelectorAll('viewer-bookmark');
    chrome.test.assertEq(2, subBookmarks.length, 'two sub bookmarks');
    chrome.test.assertEq(
        1, subBookmarks[1]!.depth, 'sub bookmark depth correct');

    let lastPageChange;
    rootBookmark.addEventListener('change-page', function(e) {
      lastPageChange = e.detail.page;
    });

    rootBookmark.$.item.click();
    await microtasksFinished();
    chrome.test.assertEq(1, lastPageChange);

    subBookmarks[1]!.$.item.click();
    await microtasksFinished();
    chrome.test.assertEq(3, lastPageChange);

    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
