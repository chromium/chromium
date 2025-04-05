// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {hexToColor, Ink2Manager, InkAnnotationTextMixin, TEXT_COLORS, TEXT_SIZES, TextAlignment, TextStyle} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {setupTestMockPluginForInk} from './test_util.js';

setupTestMockPluginForInk();
const manager = Ink2Manager.getInstance();

const TestElementBase = InkAnnotationTextMixin(CrLitElement);

class TestElement extends TestElementBase {
  static get is() {
    return 'test-element';
  }

  override render() {
    return html`
      <select @change="${this.onFontSelected}">
        <option value="Roboto"></option>
        <option value="Serif"></option>
      </select>
      <select @change="${this.onSizeSelected}">
        <option value="${TEXT_SIZES[0]}"></option>
        <option value="${TEXT_SIZES[1]}"></option>
      </select>
    `;
  }
}

customElements.define(TestElement.is, TestElement);
const testElement = document.createElement('test-element') as TestElement;
document.body.appendChild(testElement);

chrome.test.runTests([
  async function testSetProperties() {
    // Verify that calling onCurrentColorChanged with a new color calls the
    // manager and results in an event firing.
    const newColor = hexToColor(TEXT_COLORS[1]!.color);
    const colorEvent =
        new CustomEvent('current-color-changed', {detail: {value: newColor}});
    let whenChanged = eventToPromise('text-changed', manager);
    testElement.onCurrentColorChanged(colorEvent);
    let changedEvent = await whenChanged;
    chrome.test.assertEq(newColor.r, changedEvent.detail.color.r);
    chrome.test.assertEq(newColor.g, changedEvent.detail.color.g);
    chrome.test.assertEq(newColor.b, changedEvent.detail.color.b);

    // Test firing a change event from a <select> with onFontSelected
    // registered as the listener calls the manager and results in an event.
    const selects = testElement.shadowRoot.querySelectorAll('select');
    chrome.test.assertEq(2, selects.length);
    whenChanged = eventToPromise('text-changed', manager);
    const fontSelect = selects[0]!;
    fontSelect.value = 'Serif';
    fontSelect.dispatchEvent(
        new CustomEvent('change', {bubbles: true, composed: true}));
    changedEvent = await whenChanged;
    chrome.test.assertEq('Serif', changedEvent.detail.font);

    // Test firing a change event from a <select> with onSizeSelected
    // registered as the listener calls the manager and results in an event.
    whenChanged = eventToPromise('text-changed', manager);
    const sizeSelect = selects[1]!;
    sizeSelect.value = `${TEXT_SIZES[1]!}`;
    sizeSelect.dispatchEvent(
        new CustomEvent('change', {bubbles: true, composed: true}));
    changedEvent = await whenChanged;
    chrome.test.assertEq(TEXT_SIZES[1]!, changedEvent.detail.size);

    chrome.test.succeed();
  },

  function testOnTextChanged() {
    // Initial state
    const initialColor = hexToColor(TEXT_COLORS[0]!.color);
    chrome.test.assertEq(initialColor.r, testElement.currentColor.r);
    chrome.test.assertEq(initialColor.g, testElement.currentColor.g);
    chrome.test.assertEq(initialColor.b, testElement.currentColor.b);
    chrome.test.assertEq(TEXT_SIZES[0], testElement.currentSize);
    chrome.test.assertEq('', testElement.currentFont);

    const newColor = hexToColor(TEXT_COLORS[1]!.color);
    testElement.onTextChanged({
      font: 'Serif',
      size: TEXT_SIZES[1]!,
      color: newColor,
      alignment: TextAlignment.LEFT,
      styles: {
        [TextStyle.BOLD]: false,
        [TextStyle.ITALIC]: false,
        [TextStyle.UNDERLINE]: false,
        [TextStyle.STRIKETHROUGH]: false,
      },
    });
    chrome.test.assertEq(newColor.r, testElement.currentColor.r);
    chrome.test.assertEq(newColor.g, testElement.currentColor.g);
    chrome.test.assertEq(newColor.b, testElement.currentColor.b);
    chrome.test.assertEq(TEXT_SIZES[1]!, testElement.currentSize);
    chrome.test.assertEq('Serif', testElement.currentFont);

    chrome.test.succeed();
  },

  function testIsSelected() {
    // Test that isSelectedSize returns the expected value.
    chrome.test.assertTrue(testElement.isSelectedSize(TEXT_SIZES[1]!));
    chrome.test.assertFalse(testElement.isSelectedSize(TEXT_SIZES[0]!));
    testElement.currentSize = TEXT_SIZES[0]!;
    chrome.test.assertFalse(testElement.isSelectedSize(TEXT_SIZES[1]!));
    chrome.test.assertTrue(testElement.isSelectedSize(TEXT_SIZES[0]!));

    // Test that isSelectedFont returns the expected value.
    chrome.test.assertTrue(testElement.isSelectedFont('Serif'));
    chrome.test.assertFalse(testElement.isSelectedFont('Roboto'));
    testElement.currentFont = 'Roboto';
    chrome.test.assertFalse(testElement.isSelectedFont('Serif'));
    chrome.test.assertTrue(testElement.isSelectedFont('Roboto'));

    chrome.test.succeed();
  },
]);
