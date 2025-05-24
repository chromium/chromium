// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {hexToColor, Ink2Manager, InkAnnotationTextMixin, TEXT_COLORS, TEXT_SIZES, TextAlignment, TextStyle, TextTypeface} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {assertDeepEquals, setupTestMockPluginForInk} from './test_util.js';

setupTestMockPluginForInk();
const manager = Ink2Manager.getInstance();

const TestElementBase = InkAnnotationTextMixin(CrLitElement);

class TestElement extends TestElementBase {
  static get is() {
    return 'test-element';
  }

  override render() {
    return html`
      <select @change="${this.onTypefaceSelected}">
        <option value="${TextTypeface.SANS_SERIF}"></option>
        <option value="${TextTypeface.SERIF}"></option>
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
  function testInitialization() {
    // Test that the mixin initializes the fonts and other properties correctly.
    const expectedFonts = [
      TextTypeface.SANS_SERIF,
      TextTypeface.SERIF,
      TextTypeface.MONOSPACE,
    ];
    assertDeepEquals(expectedFonts, testElement.fontNames);
    assertDeepEquals({r: 0, b: 0, g: 0}, testElement.currentColor);
    chrome.test.assertEq(TextTypeface.SANS_SERIF, testElement.currentTypeface);
    chrome.test.assertEq(TEXT_SIZES[3]!, testElement.currentSize);

    chrome.test.succeed();
  },

  async function testSetProperties() {
    // Verify that calling onCurrentColorChanged with a new color calls the
    // manager and results in an event firing.
    const newColor = hexToColor(TEXT_COLORS[1]!.color);
    const colorEvent =
        new CustomEvent('current-color-changed', {detail: {value: newColor}});
    let whenChanged = eventToPromise('attributes-changed', manager);
    testElement.onCurrentColorChanged(colorEvent);
    let changedEvent = await whenChanged;
    assertDeepEquals(newColor, changedEvent.detail.color);

    // Test firing a change event from a <select> with onTypefaceSelected
    // registered as the listener calls the manager and results in an event.
    const selects = testElement.shadowRoot.querySelectorAll('select');
    chrome.test.assertEq(2, selects.length);
    whenChanged = eventToPromise('attributes-changed', manager);
    const fontSelect = selects[0]!;
    fontSelect.value = TextTypeface.SERIF;
    fontSelect.dispatchEvent(
        new CustomEvent('change', {bubbles: true, composed: true}));
    changedEvent = await whenChanged;
    chrome.test.assertEq(TextTypeface.SERIF, changedEvent.detail.typeface);

    // Test firing a change event from a <select> with onSizeSelected
    // registered as the listener calls the manager and results in an event.
    whenChanged = eventToPromise('attributes-changed', manager);
    const sizeSelect = selects[1]!;
    sizeSelect.value = `${TEXT_SIZES[1]!}`;
    sizeSelect.dispatchEvent(
        new CustomEvent('change', {bubbles: true, composed: true}));
    changedEvent = await whenChanged;
    chrome.test.assertEq(TEXT_SIZES[1]!, changedEvent.detail.size);

    chrome.test.succeed();
  },

  function testOnTextAttributesChanged() {
    // Initial state
    const initialColor = hexToColor(TEXT_COLORS[0]!.color);
    assertDeepEquals(initialColor, testElement.currentColor);
    chrome.test.assertEq(TEXT_SIZES[3]!, testElement.currentSize);
    chrome.test.assertEq(TextTypeface.SANS_SERIF, testElement.currentTypeface);

    const newColor = hexToColor(TEXT_COLORS[1]!.color);
    testElement.onTextAttributesChanged({
      typeface: TextTypeface.SERIF,
      size: TEXT_SIZES[1]!,
      color: newColor,
      alignment: TextAlignment.LEFT,
      styles: {
        [TextStyle.BOLD]: false,
        [TextStyle.ITALIC]: false,
      },
    });
    assertDeepEquals(newColor, testElement.currentColor);
    chrome.test.assertEq(TEXT_SIZES[1]!, testElement.currentSize);
    chrome.test.assertEq(TextTypeface.SERIF, testElement.currentTypeface);

    chrome.test.succeed();
  },

  function testIsSelected() {
    // Test that isSelectedSize returns the expected value.
    chrome.test.assertTrue(testElement.isSelectedSize(TEXT_SIZES[1]!));
    chrome.test.assertFalse(testElement.isSelectedSize(TEXT_SIZES[0]!));
    testElement.currentSize = TEXT_SIZES[0]!;
    chrome.test.assertFalse(testElement.isSelectedSize(TEXT_SIZES[1]!));
    chrome.test.assertTrue(testElement.isSelectedSize(TEXT_SIZES[0]!));

    // Test that isSelectedTypeface returns the expected value.
    chrome.test.assertTrue(testElement.isSelectedTypeface(TextTypeface.SERIF));
    chrome.test.assertFalse(
        testElement.isSelectedTypeface(TextTypeface.SANS_SERIF));
    testElement.currentTypeface = TextTypeface.SANS_SERIF;
    chrome.test.assertFalse(testElement.isSelectedTypeface(TextTypeface.SERIF));
    chrome.test.assertTrue(
        testElement.isSelectedTypeface(TextTypeface.SANS_SERIF));

    chrome.test.succeed();
  },
]);
