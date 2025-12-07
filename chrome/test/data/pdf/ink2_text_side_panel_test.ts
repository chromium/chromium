// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotationMode, hexToColor, Ink2Manager, TEXT_COLORS, TextTypeface, UserAction} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {Color} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {setupMockMetricsPrivate} from './test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;

chrome.test.runTests([
  // Test that toggling annotation mode opens the side panel. Must be run first,
  // as other tests expect to already be in annotation mode.
  async function testOpenSidePanel() {
    const mockMetricsPrivate = setupMockMetricsPrivate();

    viewer.$.toolbar.setAnnotationMode(AnnotationMode.TEXT);
    await Ink2Manager.getInstance().initializeTextAnnotations();
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
    chrome.test.assertTrue(!!sidePanel);

    // Font is the first select.
    const fontSelect = sidePanel.shadowRoot.querySelector('select');
    chrome.test.assertTrue(!!fontSelect);
    const initialFont =
        Ink2Manager.getInstance().getCurrentTextAttributes().typeface;
    chrome.test.assertEq(initialFont, fontSelect.value);

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
    const sidePanel = viewer.shadowRoot.querySelector('viewer-text-side-panel');
    chrome.test.assertTrue(!!sidePanel);

    // Size is the second select.
    const selects = sidePanel.shadowRoot.querySelectorAll('select');
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

  // Test that the side panel has a style select and alignment select element.
  function testHasSelectors() {
    chrome.test.assertEq(AnnotationMode.TEXT, viewer.$.toolbar.annotationMode);
    const sidePanel = viewer.shadowRoot.querySelector('viewer-text-side-panel');
    chrome.test.assertTrue(!!sidePanel);
    chrome.test.assertTrue(
        !!sidePanel.shadowRoot.querySelector('text-styles-selector'));
    chrome.test.assertTrue(
        !!sidePanel.shadowRoot.querySelector('text-alignment-selector'));
    chrome.test.succeed();
  },

  // Test that the color can be selected.
  async function testSelectColor() {
    chrome.test.assertEq(AnnotationMode.TEXT, viewer.$.toolbar.annotationMode);
    const sidePanel = viewer.shadowRoot.querySelector('viewer-text-side-panel');
    chrome.test.assertTrue(!!sidePanel);
    const colorSelector =
        sidePanel.shadowRoot.querySelector('ink-color-selector');
    chrome.test.assertTrue(!!colorSelector);

    // Initial state
    function assertColorsEqual(color1: Color, color2: Color) {
      chrome.test.assertEq(color1.r, color2.r);
      chrome.test.assertEq(color1.g, color2.g);
      chrome.test.assertEq(color1.b, color2.b);
    }
    assertColorsEqual({r: 0, g: 0, b: 0}, colorSelector.currentColor);
    const buttons = colorSelector.shadowRoot.querySelectorAll('input');
    chrome.test.assertEq(TEXT_COLORS.length, buttons.length);
    assertColorsEqual(
        {r: 0, g: 0, b: 0},
        Ink2Manager.getInstance().getCurrentTextAttributes().color);
    chrome.test.assertTrue(buttons[0]!.checked);

    // Confirm we passed all the right colors to the color selector.
    chrome.test.assertEq(TEXT_COLORS.length, colorSelector.colors.length);
    for (let i = 0; i < TEXT_COLORS.length; i++) {
      chrome.test.assertEq(
          TEXT_COLORS[i]!.label, colorSelector.colors[i]!.label);
      chrome.test.assertEq(
          TEXT_COLORS[i]!.color, colorSelector.colors[i]!.color);
      chrome.test.assertEq(false, colorSelector.colors[i]!.blended);
    }

    // Change to a different color by clicking on an unchecked button.
    const whenChanged =
        eventToPromise('attributes-changed', Ink2Manager.getInstance());
    buttons[1]!.click();
    const changedEvent = await whenChanged;
    assertColorsEqual(
        hexToColor(TEXT_COLORS[1]!.color), changedEvent.detail.color);
    await microtasksFinished();
    assertColorsEqual(
        hexToColor(TEXT_COLORS[1]!.color), colorSelector.currentColor);
    chrome.test.assertTrue(buttons[1]!.checked);

    chrome.test.succeed();
  },
]);
