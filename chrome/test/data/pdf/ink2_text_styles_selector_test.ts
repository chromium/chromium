// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Ink2Manager, TextStyle} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {CrIconButtonElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {setupTestMockPluginForInk} from './test_util.js';

setupTestMockPluginForInk();
const manager = Ink2Manager.getInstance();
const styleSelector = document.createElement('text-styles-selector');
document.body.appendChild(styleSelector);

chrome.test.runTests([
  // Test that the styles can be toggled.
  async function testSelectStyles() {
    const initialStyles = manager.getCurrentTextAttributes().styles;

    // Check that the button toggles its style and aria-pressed state and
    // triggers a attributes-changed event when clicked.
    async function testButton(
        button: CrIconButtonElement, style: TextStyle, icon: string) {
      chrome.test.assertEq(icon, button.ironIcon);
      const initialValue = initialStyles[style];
      chrome.test.assertEq(initialValue, button.classList.contains('active'));
      chrome.test.assertEq(
          initialValue.toString(), button.getAttribute('aria-pressed'));

      const whenChanged = eventToPromise('attributes-changed', manager);
      button.click();
      const changedEvent = await whenChanged;
      chrome.test.assertEq(!initialValue, changedEvent.detail.styles[style]);
      await microtasksFinished();
      chrome.test.assertEq(!initialValue, button.classList.contains('active'));
      chrome.test.assertEq(
          (!initialValue).toString(), button.getAttribute('aria-pressed'));
    }

    // For each button, check that it can be toggled and confirm it is
    // displaying the expected icon.
    const buttons = styleSelector.shadowRoot.querySelectorAll('cr-icon-button');
    chrome.test.assertEq(2, buttons.length);
    await testButton(buttons[0]!, TextStyle.BOLD, 'pdf-ink:text-format-bold');
    await testButton(
        buttons[1]!, TextStyle.ITALIC, 'pdf-ink:text-format-italic');

    chrome.test.succeed();
  },
]);
