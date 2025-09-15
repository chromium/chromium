// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ViewerBottomToolbarDropdownElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {keyDownOn, keyUpOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {getRequiredElement} from './test_util.js';

function createDropdown(): ViewerBottomToolbarDropdownElement {
  const dropdown = document.createElement('viewer-bottom-toolbar-dropdown');
  document.body.appendChild(dropdown);
  return dropdown;
}

function getMenu(dropdown: ViewerBottomToolbarDropdownElement) {
  return dropdown.shadowRoot.querySelector('slot[name="menu"]');
}

chrome.test.runTests([
  async function testButtonTogglesDropdown() {
    const dropdown = createDropdown();

    chrome.test.assertTrue(!getMenu(dropdown));

    // Open the dropdown.
    getRequiredElement(dropdown, 'cr-button').click();
    await microtasksFinished();

    chrome.test.assertTrue(!!getMenu(dropdown));
    chrome.test.succeed();
  },

  async function testFocusOnMenuDoesNotCloseDropdown() {
    const dropdown = createDropdown();

    // Open the dropdown.
    getRequiredElement(dropdown, 'cr-button').click();
    await microtasksFinished();

    const menu = getMenu(dropdown);
    chrome.test.assertTrue(!!menu);

    // Focus on the dropdown menu. The dropdown should stay visible.
    dropdown.dispatchEvent(new FocusEvent('focusout', {relatedTarget: menu}));
    await microtasksFinished();

    chrome.test.assertTrue(!!getMenu(dropdown));
    chrome.test.succeed();
  },

  async function testFocusElsewhereClosesDropdown() {
    const dropdown = createDropdown();

    // Open the dropdown.
    getRequiredElement(dropdown, 'cr-button').click();
    await microtasksFinished();

    chrome.test.assertTrue(!!getMenu(dropdown));

    // Focus on a different element. The dropdown should not be visible.
    dropdown.dispatchEvent(
        new FocusEvent('focusout', {relatedTarget: document.body}));
    await microtasksFinished();

    chrome.test.assertTrue(!getMenu(dropdown));
    chrome.test.succeed();
  },

  async function testDropdownFocusesMenuElement() {
    document.body.innerHTML = getTrustedHTML`
      <viewer-bottom-toolbar-dropdown>
        <button slot="menu">Button</button>
      </viewer-bottom-toolbar-dropdown>
    `;
    const dropdown =
        document.body.querySelector('viewer-bottom-toolbar-dropdown');
    chrome.test.assertTrue(!!dropdown);
    chrome.test.assertTrue(!getMenu(dropdown));
    const button = document.body.querySelector('button');
    chrome.test.assertTrue(!!button);
    const whenFocused = eventToPromise('focus', button);

    // Focus the dropdown and open with the keyboard.
    const crButton = getRequiredElement(dropdown, 'cr-button');
    crButton.focus();
    keyDownOn(crButton, 0, [], ' ');
    keyUpOn(crButton, 0, [], ' ');
    await microtasksFinished();
    chrome.test.assertTrue(!!getMenu(dropdown));

    // Focus should be on the button.
    await whenFocused;
    chrome.test.succeed();
  },
]);
