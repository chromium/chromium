// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PluginController, PluginControllerEventType} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {ViewerBottomToolbarDropdownElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {getRequiredElement} from './test_util.js';

function createDropdown(): ViewerBottomToolbarDropdownElement {
  const dropdown = document.createElement('viewer-bottom-toolbar-dropdown');
  document.body.appendChild(dropdown);
  return dropdown;
}

chrome.test.runTests([
  async function testButtonTogglesDropdown() {
    const dropdown = createDropdown();

    chrome.test.assertTrue(!dropdown.shadowRoot!.querySelector('slot'));

    // Open the dropdown.
    getRequiredElement(dropdown, 'cr-icon-button').click();
    await microtasksFinished();

    chrome.test.assertTrue(!!dropdown.shadowRoot!.querySelector('slot'));
    chrome.test.succeed();
  },

  async function testClickElsewhereClosesDropdown() {
    const dropdown = createDropdown();

    // Open the dropdown.
    getRequiredElement(dropdown, 'cr-icon-button').click();
    await microtasksFinished();

    chrome.test.assertTrue(!!dropdown.shadowRoot!.querySelector('slot'));

    // Click a different element. The dropdown should not be visible.
    document.body.click();
    await microtasksFinished();

    chrome.test.assertTrue(!dropdown.shadowRoot!.querySelector('slot'));
    chrome.test.succeed();
  },

  async function testFinishInkStrokeClosesDropdown() {
    const dropdown = createDropdown();

    // Open the dropdown.
    getRequiredElement(dropdown, 'cr-icon-button').click();
    await microtasksFinished();

    chrome.test.assertTrue(!!dropdown.shadowRoot!.querySelector('slot'));

    // Finish an ink stroke. The dropdown should not be visible.
    PluginController.getInstance().getEventTarget().dispatchEvent(
        new CustomEvent(PluginControllerEventType.FINISH_INK_STROKE));
    await microtasksFinished();

    chrome.test.assertTrue(!dropdown.shadowRoot!.querySelector('slot'));
    chrome.test.succeed();
  },
]);
