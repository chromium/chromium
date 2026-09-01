// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {TextAttributes} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {hexToColor, Ink2Manager, InkAnnotationTextMixin, TEXT_COLORS, TEXT_SIZES, TextAlignment, TextStyle, TextTypeface} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertDeepEquals, setupTestMockPluginForInk} from './test_util.js';

setupTestMockPluginForInk();
const manager = Ink2Manager.getInstance();

const TestDummyElementBase = InkAnnotationTextMixin(CrLitElement);

class TestDummyElement extends TestDummyElementBase {
  static get is() {
    return 'test-dummy';
  }

  override render() {
    return html`
      <select id="typefaceSelect" .value="${this.currentTypeface}"
          @change="${this.onTypefaceSelected}">
        <option value="${TextTypeface.SANS_SERIF}"></option>
        <option value="${TextTypeface.SERIF}"></option>
      </select>
      <select id="sizeSelect" .value="${this.currentSize.toString()}"
          @change="${this.onSizeSelected}">
        <option value="${TEXT_SIZES[0]}"></option>
        <option value="${TEXT_SIZES[1]}"></option>
      </select>
    `;
  }
}

customElements.define(TestDummyElement.is, TestDummyElement);
const testElement = document.createElement('test-dummy') as TestDummyElement;
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
    let whenChanged = eventToPromise<CustomEvent<TextAttributes>>(
        'attributes-changed', manager);
    testElement.onCurrentColorChanged(colorEvent);
    let changedEvent = await whenChanged;
    assertDeepEquals(newColor, changedEvent.detail.color);

    // Test firing a change event from a <select> with onTypefaceSelected
    // registered as the listener calls the manager and results in an event.
    const typefaceSelect =
        testElement.shadowRoot.querySelector<HTMLSelectElement>(
            '#typefaceSelect');
    chrome.test.assertTrue(!!typefaceSelect);
    whenChanged = eventToPromise<CustomEvent<TextAttributes>>(
        'attributes-changed', manager);
    typefaceSelect.value = TextTypeface.SERIF;
    typefaceSelect.dispatchEvent(
        new CustomEvent('change', {bubbles: true, composed: true}));
    changedEvent = await whenChanged;
    chrome.test.assertEq(TextTypeface.SERIF, changedEvent.detail.typeface);

    // Test firing a change event from a <select> with onSizeSelected
    // registered as the listener calls the manager and results in an event.
    const sizeSelect =
        testElement.shadowRoot.querySelector<HTMLSelectElement>('#sizeSelect');
    chrome.test.assertTrue(!!sizeSelect);
    whenChanged = eventToPromise<CustomEvent<TextAttributes>>(
        'attributes-changed', manager);
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
        [TextStyle.STRIKETHROUGH]: false,
      },
    });
    assertDeepEquals(newColor, testElement.currentColor);
    chrome.test.assertEq(TEXT_SIZES[1]!, testElement.currentSize);
    chrome.test.assertEq(TextTypeface.SERIF, testElement.currentTypeface);

    chrome.test.succeed();
  },

  async function testSelectValues() {
    const typefaceSelect =
        testElement.shadowRoot.querySelector<HTMLSelectElement>(
            '#typefaceSelect');
    chrome.test.assertTrue(!!typefaceSelect);
    const sizeSelect =
        testElement.shadowRoot.querySelector<HTMLSelectElement>('#sizeSelect');
    chrome.test.assertTrue(!!sizeSelect);

    // Initial values from testOnTextAttributesChanged:
    // currentTypeface = SERIF, currentSize = TEXT_SIZES[1].
    chrome.test.assertEq(TextTypeface.SERIF, typefaceSelect.value);
    chrome.test.assertEq(TEXT_SIZES[1]!.toString(), sizeSelect.value);

    // Update currentSize and currentTypeface and verify select values update.
    testElement.currentSize = TEXT_SIZES[0]!;
    testElement.currentTypeface = TextTypeface.SANS_SERIF;
    await microtasksFinished();

    chrome.test.assertEq(TextTypeface.SANS_SERIF, typefaceSelect.value);
    chrome.test.assertEq(TEXT_SIZES[0]!.toString(), sizeSelect.value);

    chrome.test.succeed();
  },
]);
