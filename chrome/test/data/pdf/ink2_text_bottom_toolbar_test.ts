// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotationMode, hexToColor, Ink2Manager, TEXT_COLORS, TextAlignment, TextTypeface, UserAction} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {Color} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assert} from 'chrome://resources/js/assert.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {clickDropdownButton, getColorButtons, getRequiredElement, setupMockMetricsPrivate} from './test_util.js';

function assertColorChipFillColor(toolbar: HTMLElement, expected: Color) {
  const expectedStyle = `rgb(${expected.r}, ${expected.g}, ${expected.b})`;
  const styles = getComputedStyle(getRequiredElement(toolbar, '.color-chip'));
  chrome.test.assertEq(
      expectedStyle, styles.getPropertyValue('background-color'));
}

const viewer = document.body.querySelector('pdf-viewer')!;

chrome.test.runTests([
  // Test that toggling annotation mode opens the toolbar. Must be run first,
  // as other tests expect to already be in annotation mode.
  async function testOpenBottomToolbar() {
    const mockMetricsPrivate = setupMockMetricsPrivate();

    viewer.$.toolbar.setAnnotationMode(AnnotationMode.TEXT);
    await Ink2Manager.getInstance().initializeTextAnnotations();
    await microtasksFinished();

    chrome.test.assertEq(AnnotationMode.TEXT, viewer.$.toolbar.annotationMode);
    chrome.test.assertTrue(isVisible(
        viewer.shadowRoot.querySelector('viewer-text-bottom-toolbar')));
    mockMetricsPrivate.assertCount(UserAction.OPEN_INK2_SIDE_PANEL, 0);
    mockMetricsPrivate.assertCount(UserAction.OPEN_INK2_BOTTOM_TOOLBAR, 1);
    chrome.test.succeed();
  },

  // Test that the bottom toolbar contains all the right selection elements.
  function testLayout() {
    chrome.test.assertEq(AnnotationMode.TEXT, viewer.$.toolbar.annotationMode);
    const toolbar =
        viewer.shadowRoot.querySelector('viewer-text-bottom-toolbar');
    assert(toolbar);

    // Font and size selects
    const selects = toolbar.shadowRoot.querySelectorAll('select');
    chrome.test.assertEq(2, selects.length);
    chrome.test.assertEq(TextTypeface.SANS_SERIF, selects[0]!.value);
    chrome.test.assertEq('12', selects[1]!.value);

    // Style selector
    chrome.test.assertTrue(
        !!toolbar.shadowRoot.querySelector('text-styles-selector'));

    // 2 toolbar dropdowns for alignment and color.
    const toolbarDropdowns =
        toolbar.shadowRoot.querySelectorAll('viewer-bottom-toolbar-dropdown');
    chrome.test.assertEq(2, toolbarDropdowns.length);
    const alignmentIcon = toolbarDropdowns[0]!.querySelector('cr-icon');
    assert(alignmentIcon);
    chrome.test.assertEq('pdf-ink:text-align-left', alignmentIcon.icon);
    assertColorChipFillColor(toolbar, hexToColor(TEXT_COLORS[0]!.color));

    chrome.test.succeed();
  },

  // Test that the font can be selected.
  async function testSelectFont() {
    chrome.test.assertEq(AnnotationMode.TEXT, viewer.$.toolbar.annotationMode);
    const toolbar =
        viewer.shadowRoot.querySelector('viewer-text-bottom-toolbar');
    assert(toolbar);

    // Font is the first select.
    const fontSelect = toolbar.shadowRoot.querySelector('select');
    assert(fontSelect);
    const initialTypeface =
        Ink2Manager.getInstance().getCurrentTextAttributes().typeface;
    chrome.test.assertEq(initialTypeface, fontSelect.value);

    const whenChanged =
        eventToPromise('attributes-changed', Ink2Manager.getInstance());
    const newValue = TextTypeface.SERIF;
    fontSelect.focus();
    fontSelect.value = newValue;
    fontSelect.dispatchEvent(new CustomEvent('change'));
    const changedEvent = await whenChanged;
    chrome.test.assertEq(newValue, changedEvent.detail.typeface);
    await microtasksFinished();
    chrome.test.assertEq(newValue, fontSelect.value);

    chrome.test.succeed();
  },

  // Test that the size can be selected.
  async function testSelectSize() {
    chrome.test.assertEq(AnnotationMode.TEXT, viewer.$.toolbar.annotationMode);
    const toolbar =
        viewer.shadowRoot.querySelector('viewer-text-bottom-toolbar');
    assert(toolbar);

    // Size is the second select.
    const selects = toolbar.shadowRoot.querySelectorAll('select');
    chrome.test.assertEq(2, selects.length);
    const sizeSelect = selects[1]!;
    const initialSize =
        Ink2Manager.getInstance().getCurrentTextAttributes().size;
    chrome.test.assertEq(initialSize.toString(), sizeSelect.value);

    const whenChanged =
        eventToPromise('attributes-changed', Ink2Manager.getInstance());
    sizeSelect.focus();
    sizeSelect.value = '20';
    sizeSelect.dispatchEvent(new CustomEvent('change'));
    const changedEvent = await whenChanged;
    chrome.test.assertEq(20, changedEvent.detail.size);
    await microtasksFinished();
    chrome.test.assertEq('20', sizeSelect.value);

    chrome.test.succeed();
  },

  // Test that the alignment can be selected.
  async function testSelectAlignment() {
    chrome.test.assertEq(AnnotationMode.TEXT, viewer.$.toolbar.annotationMode);
    const toolbar =
        viewer.shadowRoot.querySelector('viewer-text-bottom-toolbar');
    assert(toolbar);
    await clickDropdownButton(toolbar.$.alignment);
    const selector =
        toolbar.shadowRoot.querySelector('text-alignment-selector');
    assert(selector);
    const buttons =
        selector.shadowRoot.querySelectorAll('selectable-icon-button');
    chrome.test.assertEq(3, buttons.length);
    chrome.test.assertTrue(buttons[0]!.checked);

    const whenChanged =
        eventToPromise('attributes-changed', Ink2Manager.getInstance());
    buttons[1]!.click();
    const changedEvent = await whenChanged;
    chrome.test.assertEq(TextAlignment.CENTER, changedEvent.detail.alignment);
    await microtasksFinished();
    chrome.test.assertTrue(buttons[1]!.checked);
    const alignmentIcon = toolbar.$.alignment.querySelector('cr-icon');
    assert(alignmentIcon);
    chrome.test.assertEq('pdf-ink:text-align-center', alignmentIcon.icon);

    chrome.test.succeed();
  },

  // Test that the color can be selected.
  async function testSelectColor() {
    function assertColorsEqual(color1: Color, color2: Color) {
      chrome.test.assertEq(color1.r, color2.r);
      chrome.test.assertEq(color1.g, color2.g);
      chrome.test.assertEq(color1.b, color2.b);
    }

    chrome.test.assertEq(AnnotationMode.TEXT, viewer.$.toolbar.annotationMode);
    const toolbar =
        viewer.shadowRoot.querySelector('viewer-text-bottom-toolbar');
    assert(toolbar);
    await clickDropdownButton(toolbar.$.color);
    const colorSelector =
        toolbar.shadowRoot.querySelector('ink-color-selector');
    assert(colorSelector);
    assertColorsEqual(
        hexToColor(TEXT_COLORS[0]!.color), colorSelector.currentColor);

    // Change to a different color by clicking on an unchecked button.
    const colorButtons = getColorButtons(colorSelector);
    const button = colorButtons[1];
    chrome.test.assertTrue(!!button);
    const whenChanged =
        eventToPromise('attributes-changed', Ink2Manager.getInstance());
    button.click();
    const changedEvent = await whenChanged;
    assertColorsEqual(
        hexToColor(TEXT_COLORS[1]!.color), changedEvent.detail.color);
    await microtasksFinished();
    // Updated color reflected in the selector and chip.
    assertColorsEqual(
        hexToColor(TEXT_COLORS[1]!.color), colorSelector.currentColor);
    assertColorChipFillColor(toolbar, hexToColor(TEXT_COLORS[1]!.color));

    chrome.test.succeed();
  },
]);
