// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ChangePageOrigin} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';
// <if expr="not is_macosx">
// clang-format off
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
// clang-format on
// </if>

const tests = [
  // <if expr="not is_macosx">
  // TODO(crbug.com/578762077): adjust this <if> preprocessor directive
  // accordingly to include test for Mac once Mac gets its own shortcut.

  /**
   * Test the Ctrl + Alt + G shortcut for jumping to the page selector
   * for non-Mac machines.
   */
  function testJumpToPageSelectorShortcut() {
    keyDownOn(document.documentElement, 0, ['ctrl', 'alt'], 'g');
    chrome.test.assertEq('pageSelector', getDeepActiveElement()?.id);

    // Resets the focus to test the shortcut with 'G'
    (document.activeElement! as HTMLElement).blur();
    chrome.test.assertEq(document.body, getDeepActiveElement());

    keyDownOn(document.documentElement, 0, ['ctrl', 'alt'], 'G');
    chrome.test.assertEq('pageSelector', getDeepActiveElement()?.id);

    chrome.test.succeed();
  },
  // </if>

  /**
   * Test that the PDF page selector input element behaves correctly
   * for all types of input.
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
