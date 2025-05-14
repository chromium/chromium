// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Ink2Manager, TextAlignment} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {SelectableIconButtonElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {setupTestMockPluginForInk} from './test_util.js';

setupTestMockPluginForInk();
const manager = Ink2Manager.getInstance();
const alignmentSelector = document.createElement('text-alignment-selector');
document.body.appendChild(alignmentSelector);

chrome.test.runTests([
  // Test that the alignment can be selected.
  async function testSelectAlignment() {
    // Initial state
    const buttons =
        alignmentSelector.shadowRoot.querySelectorAll('selectable-icon-button');
    chrome.test.assertEq(3, buttons.length);
    chrome.test.assertEq(
        TextAlignment.LEFT, manager.getCurrentTextAttributes().alignment);
    chrome.test.assertTrue(buttons[0]!.checked);

    // Test the radio button goes from not selected to selected when it is
    // clicked.
    async function testButton(
        button: SelectableIconButtonElement, alignment: TextAlignment,
        icon: string) {
      chrome.test.assertFalse(button.checked);
      chrome.test.assertEq(icon, button.icon);

      const whenChanged = eventToPromise('attributes-changed', manager);
      button.click();
      const changedEvent = await whenChanged;
      chrome.test.assertEq(alignment, changedEvent.detail.alignment);
      await microtasksFinished();
      chrome.test.assertTrue(button.checked);
    }

    // Start with CENTER button since LEFT button is checked by default.
    await testButton(
        buttons[1]!, TextAlignment.CENTER, 'pdf-ink:text-align-center');
    await testButton(
        buttons[2]!, TextAlignment.RIGHT, 'pdf-ink:text-align-right');
    await testButton(
        buttons[0]!, TextAlignment.LEFT, 'pdf-ink:text-align-left');

    chrome.test.succeed();
  },
]);
