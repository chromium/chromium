// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotationMode, Ink2Manager, TextAlignment, TextStyle, UserAction} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {CrIconButtonElement, SelectableIconButtonElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {setupMockMetricsPrivate} from './test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;

chrome.test.runTests([
  // Test that toggling annotation mode opens the side panel. Must be run first,
  // as other tests expect to already be in annotation mode.
  async function testOpenSidePanel() {
    const mockMetricsPrivate = setupMockMetricsPrivate();

    // Enable text annotations.
    loadTimeData.overrideValues({'pdfTextAnnotationsEnabled': true});
    viewer.$.toolbar.strings = Object.assign({}, viewer.$.toolbar.strings);
    await microtasksFinished();

    viewer.$.toolbar.setAnnotationMode(AnnotationMode.TEXT);
    await microtasksFinished();

    chrome.test.assertEq(AnnotationMode.TEXT, viewer.$.toolbar.annotationMode);
    chrome.test.assertTrue(
        isVisible(viewer.shadowRoot.querySelector('viewer-text-side-panel')));
    mockMetricsPrivate.assertCount(UserAction.OPEN_INK2_SIDE_PANEL, 1);
    mockMetricsPrivate.assertCount(UserAction.OPEN_INK2_BOTTOM_TOOLBAR, 0);
    chrome.test.succeed();
  },

  // Test that the font can be selected.
  async function testSelectFont() {
    chrome.test.assertEq(AnnotationMode.TEXT, viewer.$.toolbar.annotationMode);
    const sidePanel = viewer.shadowRoot.querySelector('viewer-text-side-panel');
    assert(sidePanel);

    // Font is the first select.
    const fontSelect = sidePanel.shadowRoot.querySelector('select');
    assert(fontSelect);
    const initialFont = Ink2Manager.getInstance().getCurrentText().font;
    chrome.test.assertEq(initialFont, fontSelect.value);
    chrome.test.assertEq(initialFont, fontSelect.style.fontFamily);

    const whenChanged =
        eventToPromise('text-changed', Ink2Manager.getInstance());
    const newValue = 'Serif';
    fontSelect.focus();
    fontSelect.value = newValue;
    fontSelect.dispatchEvent(new CustomEvent('change'));
    const changedEvent = await whenChanged;
    chrome.test.assertEq(newValue, changedEvent.detail.font);
    await microtasksFinished();
    chrome.test.assertEq(newValue, fontSelect.value);
    chrome.test.assertEq(newValue, fontSelect.style.fontFamily);

    chrome.test.succeed();
  },

  // Test that the size can be selected.
  async function testSelectSize() {
    chrome.test.assertEq(AnnotationMode.TEXT, viewer.$.toolbar.annotationMode);
    const sidePanel = viewer.shadowRoot.querySelector('viewer-text-side-panel');
    assert(sidePanel);

    // Size is the second select.
    const selects = sidePanel.shadowRoot.querySelectorAll('select');
    chrome.test.assertEq(2, selects.length);
    const sizeSelect = selects[1]!;
    const initialSize = Ink2Manager.getInstance().getCurrentText().size;
    chrome.test.assertEq(initialSize.toString(), sizeSelect.value);

    const whenChanged =
        eventToPromise('text-changed', Ink2Manager.getInstance());
    sizeSelect.focus();
    sizeSelect.value = '20';
    sizeSelect.dispatchEvent(new CustomEvent('change'));
    const changedEvent = await whenChanged;
    chrome.test.assertEq(20, changedEvent.detail.size);
    await microtasksFinished();
    chrome.test.assertEq('20', sizeSelect.value);

    chrome.test.succeed();
  },

  // Test that the styles can be toggled.
  async function testSelectStyles() {
    chrome.test.assertEq(AnnotationMode.TEXT, viewer.$.toolbar.annotationMode);
    const sidePanel = viewer.shadowRoot.querySelector('viewer-text-side-panel');
    assert(sidePanel);

    const initialStyles = Ink2Manager.getInstance().getCurrentText().styles;

    // Check that the button toggles its style and aria-pressed state and
    // triggers a text-changed event when clicked.
    async function testButton(
        button: CrIconButtonElement, style: TextStyle, icon: string) {
      chrome.test.assertEq(icon, button.ironIcon);
      const initialValue = initialStyles[style];
      chrome.test.assertEq(initialValue, button.classList.contains('active'));
      chrome.test.assertEq(
          initialValue.toString(), button.getAttribute('aria-pressed'));

      const whenChanged =
          eventToPromise('text-changed', Ink2Manager.getInstance());
      button.click();
      const changedEvent = await whenChanged;
      chrome.test.assertEq(!initialValue, changedEvent.detail.styles[style]);
      await microtasksFinished();
      chrome.test.assertEq(!initialValue, button.classList.contains('active'));
      chrome.test.assertEq(
          (!initialValue).toString(), button.getAttribute('aria-pressed'));
    }

    // For each button, check that it can be toggled and change the styles.
    const buttons = sidePanel.shadowRoot.querySelectorAll('cr-icon-button');
    chrome.test.assertEq(4, buttons.length);
    await testButton(buttons[0]!, TextStyle.BOLD, 'pdf:text-format-bold');
    await testButton(buttons[1]!, TextStyle.ITALIC, 'pdf:text-format-italic');
    await testButton(
        buttons[2]!, TextStyle.UNDERLINE, 'pdf:text-format-underline');
    await testButton(
        buttons[3]!, TextStyle.STRIKETHROUGH, 'pdf:text-format-strikethrough');

    chrome.test.succeed();
  },

  // Test that the alignment can be selected.
  async function testSelectAlignment() {
    chrome.test.assertEq(AnnotationMode.TEXT, viewer.$.toolbar.annotationMode);
    const sidePanel = viewer.shadowRoot.querySelector('viewer-text-side-panel');
    assert(sidePanel);

    // Initial state
    const buttons =
        sidePanel.shadowRoot.querySelectorAll('selectable-icon-button');
    chrome.test.assertEq(4, buttons.length);
    chrome.test.assertEq(
        TextAlignment.LEFT,
        Ink2Manager.getInstance().getCurrentText().alignment);
    chrome.test.assertTrue(buttons[0]!.checked);

    // Test the radio button goes from not selected to selected when it is
    // clicked.
    async function testButton(
        button: SelectableIconButtonElement, alignment: TextAlignment,
        icon: string) {
      chrome.test.assertFalse(button.checked);
      chrome.test.assertEq(icon, button.icon);

      const whenChanged =
          eventToPromise('text-changed', Ink2Manager.getInstance());
      button.click();
      const changedEvent = await whenChanged;
      chrome.test.assertEq(alignment, changedEvent.detail.alignment);
      await microtasksFinished();
      chrome.test.assertTrue(button.checked);
    }

    // Start with CENTER button since LEFT button is checked by default.
    await testButton(
        buttons[1]!, TextAlignment.CENTER, 'pdf:text-align-center');
    await testButton(
        buttons[2]!, TextAlignment.JUSTIFY, 'pdf:text-align-justify');
    await testButton(buttons[3]!, TextAlignment.RIGHT, 'pdf:text-align-right');
    await testButton(buttons[0]!, TextAlignment.LEFT, 'pdf:text-align-left');

    chrome.test.succeed();
  },
]);
