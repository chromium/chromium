// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {hexToColor, Ink2Manager, TEXT_COLORS, TextAlignment, TextStyle} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {setupTestMockPluginForInk} from './test_util.js';

setupTestMockPluginForInk();
const manager = Ink2Manager.getInstance();
// Initialize a font, since this starts out empty.
manager.setTextFont('Roboto');
const textbox = document.createElement('ink-text-box');
document.body.appendChild(textbox);

chrome.test.runTests([
  // Test drawing the box based on position from the backend.
  async function testDrawsBox() {
    // Initial state
    chrome.test.assertTrue(textbox.hidden);
    chrome.test.assertFalse(isVisible(textbox));

    function assertPositionAndSize(
        el: HTMLElement, expectedHeight: string, expectedWidth: string,
        expectedLeft: string, expectedTop: string) {
      const styles = getComputedStyle(el);
      chrome.test.assertEq(expectedHeight, styles.getPropertyValue('height'));
      chrome.test.assertEq(expectedWidth, styles.getPropertyValue('width'));
      chrome.test.assertEq(expectedLeft, styles.getPropertyValue('left'));
      chrome.test.assertEq(expectedTop, styles.getPropertyValue('top'));
    }

    // Create a 160x40 box at 80, 120.
    manager.dispatchEvent(new CustomEvent(
        'update-text-box',
        {detail: {height: 40, locationX: 80, locationY: 120, width: 160}}));
    await microtasksFinished();
    chrome.test.assertFalse(textbox.hidden);
    chrome.test.assertTrue(isVisible(textbox));
    assertPositionAndSize(textbox, '40px', '160px', '80px', '120px');

    // Update to a 100x200 box at 400, 300.
    manager.dispatchEvent(new CustomEvent(
        'update-text-box',
        {detail: {height: 200, locationX: 400, locationY: 300, width: 100}}));
    await microtasksFinished();
    chrome.test.assertFalse(textbox.hidden);
    chrome.test.assertTrue(isVisible(textbox));
    assertPositionAndSize(textbox, '200px', '100px', '400px', '300px');

    manager.dispatchEvent(new CustomEvent(
        'update-text-box',
        {detail: {height: 0, locationX: 400, locationY: 300, width: 0}}));
    await microtasksFinished();
    chrome.test.assertTrue(textbox.hidden);
    chrome.test.assertFalse(isVisible(textbox));
    chrome.test.succeed();
  },

  // Test that the textbox styles change based on an update event.
  async function testTextbox() {
    // Update to a 100x200 box at 400, 300.
    manager.dispatchEvent(new CustomEvent(
        'update-text-box',
        {detail: {height: 200, locationX: 400, locationY: 300, width: 100}}));
    await microtasksFinished();
    chrome.test.assertFalse(textbox.hidden);
    chrome.test.assertTrue(isVisible(textbox));

    chrome.test.assertEq('Sample Text', textbox.$.textbox.value);
    const textboxStyles = getComputedStyle(textbox.$.textbox);

    // Initial state
    chrome.test.assertEq('12px', textboxStyles.getPropertyValue('font-size'));
    chrome.test.assertEq(
        'Roboto', textboxStyles.getPropertyValue('font-family'));
    chrome.test.assertEq('400', textboxStyles.getPropertyValue('font-weight'));
    chrome.test.assertEq(
        'normal', textboxStyles.getPropertyValue('font-style'));
    chrome.test.assertEq('left', textboxStyles.getPropertyValue('text-align'));
    chrome.test.assertTrue(
        textboxStyles.getPropertyValue('text-decoration').includes('none'));
    const color = hexToColor(TEXT_COLORS[0]!.color);
    const colorStyle = `rgb(${color.r}, ${color.g}, ${color.b})`;
    chrome.test.assertEq(colorStyle, textboxStyles.getPropertyValue('color'));

    // Confirm updating styles in the manager updates the style of the textbox.
    manager.setTextFont('Serif');
    manager.setTextSize(20);
    manager.setTextStyles({
      [TextStyle.BOLD]: true,
      [TextStyle.ITALIC]: true,
      [TextStyle.UNDERLINE]: true,
      [TextStyle.STRIKETHROUGH]: false,
    });
    const newColor = hexToColor(TEXT_COLORS[1]!.color);
    manager.setTextColor(newColor);
    manager.setTextAlignment(TextAlignment.RIGHT);
    await microtasksFinished();
    chrome.test.assertEq('20px', textboxStyles.getPropertyValue('font-size'));
    chrome.test.assertEq(
        'serif', textboxStyles.getPropertyValue('font-family'));
    chrome.test.assertEq('700', textboxStyles.getPropertyValue('font-weight'));
    chrome.test.assertEq(
        'italic', textboxStyles.getPropertyValue('font-style'));
    chrome.test.assertEq('right', textboxStyles.getPropertyValue('text-align'));
    chrome.test.assertTrue(textboxStyles.getPropertyValue('text-decoration')
                               .includes('underline'));
    const newColorStyle = `rgb(${newColor.r}, ${newColor.g}, ${newColor.b})`;
    chrome.test.assertEq(
        newColorStyle, textboxStyles.getPropertyValue('color'));

    chrome.test.succeed();
  },
]);
