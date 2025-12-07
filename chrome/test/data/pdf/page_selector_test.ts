// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ChangePageOrigin} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

const tests = [
  /**
   * Test that the toolbar shows an option to download the edited PDF if
   * available.
   */
  async function testPageSelector() {
    document.body.innerHTML = '';
    const selector = document.createElement('viewer-page-selector');
    document.body.appendChild(selector);
    selector.docLength = 10;
    await selector.updateComplete;

    // Check that the page number property is reflected in the input.
    chrome.test.assertEq('1', selector.$.pageSelector.value);
    selector.pageNo = 5;
    await selector.updateComplete;
    chrome.test.assertEq('5', selector.$.pageSelector.value);

    // Setting non-digit characters causes them to be immediately removed.
    selector.$.pageSelector.value = 'a';
    selector.$.pageSelector.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));
    await selector.updateComplete;
    chrome.test.assertEq('', selector.$.pageSelector.value);

    // Setting and committing a page number should fire an event.
    selector.$.pageSelector.value = '3';
    selector.$.pageSelector.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));
    const whenPageChange = eventToPromise('change-page', selector);
    selector.$.pageSelector.dispatchEvent(
        new CustomEvent('change', {bubbles: true, composed: true}));
    const e = await whenPageChange;
    chrome.test.assertEq(2, e.detail.page);
    chrome.test.assertEq(ChangePageOrigin.PAGE_SELECTOR, e.detail.origin);

    // Setting the page number after having set the value from an input
    // event should also be reflected in the input.
    selector.pageNo = 2;
    await selector.updateComplete;
    chrome.test.assertEq('2', selector.$.pageSelector.value);
    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
