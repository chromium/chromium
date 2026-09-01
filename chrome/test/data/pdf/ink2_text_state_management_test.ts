// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotationMode, hexToColor, Ink2Manager, PdfViewerPrivateProxyImpl, PluginController, PluginControllerEventType, TextAlignment} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {InkColorSelectorElement, InkTextAnnotationsElement, InkTextBoxElement, TextAlignmentSelectorElement, TextAnnotationMessageData, TextAttributes, ViewerBottomToolbarDropdownElement, ViewerTextBottomToolbarElement, ViewerTextSidePanelElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {getNewTestBeforeUnloadProxy} from './test_before_unload_proxy.js';
import {TestPdfViewerPrivateProxy} from './test_pdf_viewer_private_proxy.js';
import {clickDropdownButton, setupTestMockPluginForInk} from './test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;
const mockPlugin = setupTestMockPluginForInk();
const privateProxy = new TestPdfViewerPrivateProxy();
PdfViewerPrivateProxyImpl.setInstance(privateProxy);
getNewTestBeforeUnloadProxy();

function dispatchSendClickEvent(x: number, y: number) {
  PluginController.getInstance().getEventTarget().dispatchEvent(new CustomEvent(
      PluginControllerEventType.PLUGIN_MESSAGE,
      {detail: {type: 'sendClickEvent', x, y}}));
}

function getPagePoint(offsetX: number, offsetY: number) {
  const pageRect = viewer.viewport.getPageScreenRect(0);
  return {x: pageRect.x + offsetX, y: pageRect.y + offsetY};
}

function getOutsidePagePoint() {
  const pageRect = viewer.viewport.getPageScreenRect(0);
  return {x: pageRect.x + 350, y: pageRect.y + 350};
}

function getTextAnnotations(): InkTextAnnotationsElement {
  const annotations = viewer.shadowRoot.querySelector('ink-text-annotations');
  chrome.test.assertTrue(!!annotations);
  return annotations;
}

function getTextBox(): InkTextBoxElement {
  return getTextAnnotations().$.textBox;
}

function getTextPanel(): ViewerTextSidePanelElement|
    ViewerTextBottomToolbarElement {
  const sidePanel = viewer.shadowRoot.querySelector('viewer-text-side-panel');
  if (sidePanel) {
    return sidePanel;
  }
  const bottomToolbar =
      viewer.shadowRoot.querySelector('viewer-text-bottom-toolbar');
  chrome.test.assertTrue(!!bottomToolbar);
  return bottomToolbar;
}

function getSizeSelect(): HTMLSelectElement {
  const sizeSelect =
      getTextPanel().shadowRoot.querySelector<HTMLSelectElement>('#sizeSelect');
  chrome.test.assertTrue(!!sizeSelect);
  return sizeSelect;
}

function getColorSelector(): InkColorSelectorElement {
  const colorSelector =
      getTextPanel().shadowRoot.querySelector('ink-color-selector');
  chrome.test.assertTrue(!!colorSelector);
  return colorSelector;
}

function getAlignmentSelector(): TextAlignmentSelectorElement {
  const alignmentSelector =
      getTextPanel().shadowRoot.querySelector('text-alignment-selector');
  chrome.test.assertTrue(!!alignmentSelector);
  return alignmentSelector;
}

async function selectAlignment(alignment: TextAlignment) {
  const panel = getTextPanel();
  const alignmentDropdown =
      panel.shadowRoot.querySelector<ViewerBottomToolbarDropdownElement>(
          '#alignment');
  if (alignmentDropdown &&
      !alignmentDropdown.shadowRoot.querySelector('slot[name="menu"]')) {
    await clickDropdownButton(alignmentDropdown);
  }
  const button = getAlignmentSelector().shadowRoot.querySelector<HTMLElement>(
      `selectable-icon-button[name="${alignment}"]`);
  chrome.test.assertTrue(!!button);
  const whenChanged = eventToPromise<CustomEvent<TextAttributes>>(
      'attributes-changed', Ink2Manager.getInstance());
  button.click();
  await whenChanged;
  await microtasksFinished();
}

async function selectColorByHex(hex: string) {
  const panel = getTextPanel();
  const colorDropdown =
      panel.shadowRoot.querySelector<ViewerBottomToolbarDropdownElement>(
          '#color');
  if (colorDropdown &&
      !colorDropdown.shadowRoot.querySelector('slot[name="menu"]')) {
    await clickDropdownButton(colorDropdown);
  }
  const colorButton =
      getColorSelector().shadowRoot.querySelector<HTMLInputElement>(
          `input[value="${hex}"]`);
  chrome.test.assertTrue(!!colorButton);
  const whenChanged = eventToPromise<CustomEvent<TextAttributes>>(
      'attributes-changed', Ink2Manager.getInstance());
  colorButton.click();
  await whenChanged;
  await microtasksFinished();
}

function getCurrentAlignment(): TextAlignment {
  const radioGroup =
      getAlignmentSelector().shadowRoot.querySelector('cr-radio-group');
  chrome.test.assertTrue(!!radioGroup);
  return radioGroup.selected as TextAlignment;
}

function getFinishMessage(): TextAnnotationMessageData|undefined {
  const msg =
      mockPlugin.findMessage<{type: string, data: TextAnnotationMessageData}>(
          'finishTextAnnotation');
  return msg?.data;
}

chrome.test.runTests([
  // Tests that selecting a font size (e.g. 40) persists when an annotation is
  // edited, committed, and subsequently re-activated for editing.
  async function testFontSizeStateManagement() {
    viewer.$.toolbar.setAnnotationMode(AnnotationMode.TEXT);
    await Ink2Manager.getInstance().initializeTextAnnotations();
    await microtasksFinished();

    const panel = getTextPanel();
    chrome.test.assertTrue(!!panel);
    chrome.test.assertTrue(isVisible(panel));

    const textbox = getTextBox();
    const sizeSelect = getSizeSelect();

    const annotCreatePoint = getPagePoint(100, 100);
    const annotReactivatePoint = getPagePoint(120, 105);
    const outsidePoint = getOutsidePagePoint();

    // 1. Create a new text annotation at (100, 100).
    dispatchSendClickEvent(annotCreatePoint.x, annotCreatePoint.y);
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));

    // Type text "Hello".
    textbox.$.textbox.value = 'Hello';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();

    // 2. Commit annotation by clicking outside on empty space.
    mockPlugin.clearMessages();
    dispatchSendClickEvent(outsidePoint.x, outsidePoint.y);
    await microtasksFinished();

    chrome.test.assertFalse(isVisible(textbox));
    const finishMessage1 = getFinishMessage();
    chrome.test.assertTrue(!!finishMessage1);
    chrome.test.assertEq('Hello', finishMessage1.text);
    chrome.test.assertEq(12, finishMessage1.textAttributes.size);

    // 3. Re-activate the annotation by clicking on it.
    mockPlugin.clearMessages();
    dispatchSendClickEvent(annotReactivatePoint.x, annotReactivatePoint.y);
    await microtasksFinished();

    chrome.test.assertTrue(isVisible(textbox));
    chrome.test.assertEq('Hello', textbox.$.textbox.value);
    chrome.test.assertEq('12', sizeSelect.value);

    // 4. Change font size to 40 in the side panel.
    sizeSelect.focus();
    sizeSelect.value = '40';
    sizeSelect.dispatchEvent(new CustomEvent('change'));
    await microtasksFinished();

    chrome.test.assertEq('40', sizeSelect.value);
    chrome.test.assertEq(
        40, Ink2Manager.getInstance().getCurrentTextAttributes().size);

    // 5. Commit annotation with size 40 by clicking outside.
    mockPlugin.clearMessages();
    dispatchSendClickEvent(outsidePoint.x, outsidePoint.y);
    await microtasksFinished();

    chrome.test.assertFalse(isVisible(textbox));
    const finishMessage2 = getFinishMessage();
    chrome.test.assertTrue(!!finishMessage2);
    chrome.test.assertEq('Hello', finishMessage2.text);
    chrome.test.assertEq(40, finishMessage2.textAttributes.size);

    // 6. Re-activate the annotation again by clicking at its location.
    mockPlugin.clearMessages();
    dispatchSendClickEvent(annotReactivatePoint.x, annotReactivatePoint.y);
    await microtasksFinished();

    // Verify text size is 40 and the side panel dropdown displays "40".
    chrome.test.assertTrue(isVisible(textbox));
    chrome.test.assertEq('Hello', textbox.$.textbox.value);
    chrome.test.assertEq('40', sizeSelect.value);
    chrome.test.assertEq(
        40, Ink2Manager.getInstance().getCurrentTextAttributes().size);

    // Clean up
    await textbox.commitTextAnnotation();
    await microtasksFinished();
    chrome.test.succeed();
  },

  // Tests that switching directly from Annotation 1 (Red, Left-aligned) into
  // Annotation 2 (Blue, Center-aligned) properly preserves Annotation 2's
  // color and alignment without being overwritten by default tool settings
  // (Green, Right-aligned).
  async function testColorAndAlignmentStateManagementOnAnnotationSwitch() {
    viewer.$.toolbar.setAnnotationMode(AnnotationMode.TEXT);
    await Ink2Manager.getInstance().initializeTextAnnotations();
    await microtasksFinished();

    const textbox = getTextBox();
    const colorSelector = getColorSelector();

    const RED_HEX = '#f28b82';
    const BLUE_HEX = '#8ab4f8';
    const GREEN_HEX = '#81c995';

    const annot1CreatePoint = getPagePoint(100, 300);
    const annot1ReactivatePoint = getPagePoint(120, 305);
    const annot2CreatePoint = getPagePoint(200, 500);
    const annot2ReactivatePoint = getPagePoint(220, 505);
    const outsidePoint = getOutsidePagePoint();

    // 1. Select RED color for Annotation 1 (default alignment is already LEFT).
    await selectColorByHex(RED_HEX);
    await microtasksFinished();

    // Create Annotation 1.
    dispatchSendClickEvent(annot1CreatePoint.x, annot1CreatePoint.y);
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));

    textbox.$.textbox.value = 'Hello';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();

    // Commit Annotation 1 by clicking outside.
    mockPlugin.clearMessages();
    dispatchSendClickEvent(outsidePoint.x, outsidePoint.y);
    await microtasksFinished();

    const finishMsg1 = getFinishMessage();
    chrome.test.assertTrue(!!finishMsg1);
    chrome.test.assertEq(hexToColor(RED_HEX), finishMsg1.textAttributes.color);
    chrome.test.assertEq(
        TextAlignment.LEFT, finishMsg1.textAttributes.alignment);

    // 2. Select BLUE color and CENTER alignment for Annotation 2.
    await selectColorByHex(BLUE_HEX);
    await selectAlignment(TextAlignment.CENTER);
    await microtasksFinished();

    // Create Annotation 2.
    dispatchSendClickEvent(annot2CreatePoint.x, annot2CreatePoint.y);
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));

    textbox.$.textbox.value = 'World';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();

    // Commit Annotation 2 by clicking outside.
    mockPlugin.clearMessages();
    dispatchSendClickEvent(outsidePoint.x, outsidePoint.y);
    await microtasksFinished();

    const finishMsg2 = getFinishMessage();
    chrome.test.assertTrue(!!finishMsg2);
    chrome.test.assertEq(hexToColor(BLUE_HEX), finishMsg2.textAttributes.color);
    chrome.test.assertEq(
        TextAlignment.CENTER, finishMsg2.textAttributes.alignment);

    // 3. Select GREEN and RIGHT alignment as default settings for new
    // annotations.
    await selectColorByHex(GREEN_HEX);
    await selectAlignment(TextAlignment.RIGHT);
    await microtasksFinished();
    chrome.test.assertEq(hexToColor(GREEN_HEX), colorSelector.currentColor);
    chrome.test.assertEq(TextAlignment.RIGHT, getCurrentAlignment());

    // 4. Click into Annotation 1 (Red, Left-aligned).
    dispatchSendClickEvent(annot1ReactivatePoint.x, annot1ReactivatePoint.y);
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));
    chrome.test.assertEq('Hello', textbox.$.textbox.value);
    chrome.test.assertEq(hexToColor(RED_HEX), colorSelector.currentColor);
    chrome.test.assertEq(TextAlignment.LEFT, getCurrentAlignment());
    chrome.test.assertEq(
        hexToColor(RED_HEX),
        Ink2Manager.getInstance().getCurrentTextAttributes().color);
    chrome.test.assertEq(
        TextAlignment.LEFT,
        Ink2Manager.getInstance().getCurrentTextAttributes().alignment);

    // 5. Click directly into Annotation 2 (Blue, Center-aligned).
    mockPlugin.clearMessages();
    dispatchSendClickEvent(annot2ReactivatePoint.x, annot2ReactivatePoint.y);
    await microtasksFinished();

    // Annotation 2 is now active.
    chrome.test.assertTrue(isVisible(textbox));
    chrome.test.assertEq('World', textbox.$.textbox.value);
    // Verify that the color and alignment displayed in the panel are BLUE and
    // CENTER (and not Green / Right).
    chrome.test.assertEq(hexToColor(BLUE_HEX), colorSelector.currentColor);
    chrome.test.assertEq(TextAlignment.CENTER, getCurrentAlignment());
    chrome.test.assertEq(
        hexToColor(BLUE_HEX),
        Ink2Manager.getInstance().getCurrentTextAttributes().color);
    chrome.test.assertEq(
        TextAlignment.CENTER,
        Ink2Manager.getInstance().getCurrentTextAttributes().alignment);

    // 6. Commit Annotation 2 by clicking outside.
    mockPlugin.clearMessages();
    dispatchSendClickEvent(outsidePoint.x, outsidePoint.y);
    await microtasksFinished();

    // Verify Annotation 2 committed with BLUE and CENTER.
    const finishMsg3 = getFinishMessage();
    chrome.test.assertTrue(!!finishMsg3);
    chrome.test.assertEq('World', finishMsg3.text);
    chrome.test.assertEq(hexToColor(BLUE_HEX), finishMsg3.textAttributes.color);
    chrome.test.assertEq(
        TextAlignment.CENTER, finishMsg3.textAttributes.alignment);

    // Verify panel reverted back to default GREEN and RIGHT alignment.
    chrome.test.assertEq(hexToColor(GREEN_HEX), colorSelector.currentColor);
    chrome.test.assertEq(TextAlignment.RIGHT, getCurrentAlignment());
    chrome.test.assertEq(
        hexToColor(GREEN_HEX),
        Ink2Manager.getInstance().getCurrentTextAttributes().color);
    chrome.test.assertEq(
        TextAlignment.RIGHT,
        Ink2Manager.getInstance().getCurrentTextAttributes().alignment);

    chrome.test.succeed();
  },
]);
