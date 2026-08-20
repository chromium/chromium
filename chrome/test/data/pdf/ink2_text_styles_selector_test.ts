// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Ink2Manager, TextStyle} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {CrIconButtonElement, TextAttributes} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {setupTestMockPluginForInk} from './test_util.js';

setupTestMockPluginForInk();
const manager = Ink2Manager.getInstance();

chrome.test.runTests([
  // Test that only default styles (bold, italic) are shown when extra styles
  // are disabled.
  async function testDefaultStyles() {
    loadTimeData.overrideValues(
        {'pdfTextAnnotationsExtraStylesEnabled': false});
    const styleSelector = document.createElement('text-styles-selector');
    document.body.appendChild(styleSelector);
    await microtasksFinished();

    const buttons = styleSelector.shadowRoot.querySelectorAll('cr-icon-button');
    chrome.test.assertEq(2, buttons.length);
    chrome.test.assertEq('pdf-ink:format-bold', buttons[0]!.ironIcon);
    chrome.test.assertEq('pdf-ink:format-italic', buttons[1]!.ironIcon);

    styleSelector.remove();
    chrome.test.succeed();
  },

  // Test that extra styles (strikethrough) are shown and can be toggled when
  // extra styles are enabled.
  async function testSelectStyles() {
    loadTimeData.overrideValues({'pdfTextAnnotationsExtraStylesEnabled': true});
    const styleSelector = document.createElement('text-styles-selector');
    document.body.appendChild(styleSelector);
    await microtasksFinished();

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

      const whenChanged = eventToPromise<CustomEvent<TextAttributes>>(
          'attributes-changed', manager);
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
    chrome.test.assertEq(3, buttons.length);
    await testButton(buttons[0]!, TextStyle.BOLD, 'pdf-ink:format-bold');
    await testButton(buttons[1]!, TextStyle.ITALIC, 'pdf-ink:format-italic');
    await testButton(
        buttons[2]!, TextStyle.STRIKETHROUGH, 'pdf-ink:strikethrough-s');

    styleSelector.remove();
    chrome.test.succeed();
  },
]);
