// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {removeExtraneousElementsFrom} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import { assertEquals, assertNotEquals, assertFalse, assertTrue } from 'chrome-untrusted://webui-test/chai_assert.js';

suite('Remove Extraneous Elements', function() {
  let testContainer: HTMLElement;

  function addElement(
      parent: HTMLElement,
      tag: string,
      options:
          {text?: string, id?: string, attrs?: Record<string, string>} = {},
      ): HTMLElement {
    const el = document.createElement(tag);
    if (options.text) {
      el.textContent = options.text;
    }
    if (options.id) {
      el.id = options.id;
    }
    if (options.attrs) {
      for (const [key, value] of Object.entries(options.attrs)) {
        el.setAttribute(key, value);
      }
    }
    parent.appendChild(el);
    return el;
  }

  setup(() => {
    testContainer = document.createElement('div');
    document.body.appendChild(testContainer);
  });

  teardown(() => {
    document.body.removeChild(testContainer);
  });

  test('removes UI labels', () => {
    addElement(testContainer, 'div', {text: 'Advertisement'});
    addElement(testContainer, 'p', {text: 'Sponsored'});
    addElement(testContainer, 'span', {text: 'Supported by'});
    addElement(testContainer, 'article', {text: 'Skip Advertisement'});
    addElement(testContainer, 'div', {
      text:
          'Legitimate content with advertisement inside a long paragraph that exceeds the forty character limit by quite a bit.',
    });

    removeExtraneousElementsFrom(testContainer);

    const remainingDivs = testContainer.querySelectorAll('div');
    assertEquals(1, remainingDivs.length);
    assertTrue(
        (remainingDivs[0]?.textContent || '').includes('Legitimate content'));
    assertEquals(0, testContainer.querySelectorAll('p').length);
    assertEquals(0, testContainer.querySelectorAll('span').length);
    assertEquals(0, testContainer.querySelectorAll('article').length);
  });

  test('removes accessibility announcements', () => {
    addElement(
        testContainer, 'div',
        {text: 'Loading...', attrs: {'aria-live': 'polite'}});
    addElement(testContainer, 'div', {text: 'Error', attrs: {'role': 'alert'}});
    addElement(
        testContainer, 'div', {text: 'Saved', attrs: {'role': 'status'}});
    addElement(
        testContainer, 'div', {text: 'Log entry', attrs: {'role': 'log'}});
    addElement(testContainer, 'div', {
      text:
          'This is a very long live blog entry that should not be removed because it is well over the two hundred character limit. It contains actual content that users will want to read during a live event. We are adding more text to ensure it surpasses the length threshold reliably.',
      attrs: {'aria-live': 'polite'},
    });

    removeExtraneousElementsFrom(testContainer);

    const remainingDivs = testContainer.querySelectorAll('div');
    assertEquals(1, remainingDivs.length);
    assertTrue(
        (remainingDivs[0]?.textContent || '').includes('very long live blog'));
  });

  test('removes visually hidden empty elements', () => {
    // Removed: 0x0 and no text. It's completely empty.
    addElement(testContainer, 'div', {attrs: {style: 'width: 0; height: 0;'}});

    // Removed: 0x0 and its text is just a whitespace (\u00A0).
    addElement(
        testContainer, 'span',
        {text: '\u00A0', attrs: {style: 'width: 0; height: 0;'}});

    // Kept: 0x0 but contains actual text ('Text') that shouldn't be lost.
    addElement(
        testContainer, 'div',
        {text: 'Text', attrs: {style: 'width: 0; height: 0;'}});

    // Kept: Has a visible width (10px), so it's not considered 100% hidden.
    addElement(
        testContainer, 'div', {attrs: {style: 'width: 10px; height: 0;'}});

    // Kept: It's an image (<img> tag), not just an empty text container.
    addElement(testContainer, 'img', {attrs: {style: 'width: 0; height: 0;'}});

    // Kept: Height 0 and float, assumes default visible width (not 100% hidden).
    addElement(
        testContainer, 'div', {attrs: {style: 'float: left; height: 0;'}});

    // Simulate getBoundingClientRect for testing
    const elements = testContainer.querySelectorAll('*');
    for (const el of elements) {
      const htmlEl = el as HTMLElement;
      htmlEl.getBoundingClientRect = function() {
        const style = this.getAttribute('style') || '';
        return {
          width: style.includes('width: 0') ? 0 : 10,
          height: style.includes('height: 0') ? 0 : 10,
          top: 0,
          left: 0,
          bottom: 0,
          right: 0,
          x: 0,
          y: 0,
          toJSON: () => {},
        } as DOMRect;
      };
    }

    removeExtraneousElementsFrom(testContainer);

    const remainingElements = testContainer.querySelectorAll('*');
    assertEquals(4, remainingElements.length);
    assertEquals('DIV', remainingElements[0]?.tagName);
    assertEquals('Text', remainingElements[0]?.textContent);
    assertEquals('DIV', remainingElements[1]?.tagName);
    assertEquals('IMG', remainingElements[2]?.tagName);
    assertEquals('DIV', remainingElements[3]?.tagName);
  });

  test('removes nested extraneous elements correctly', () => {
    const keep1 = addElement(testContainer, 'div', {
      id: 'keep1',
      text:
          'This is a very long paragraph that will definitely exceed the one hundred character limit to ensure parent is kept. Keep Me ',
    });

    const remove1 = addElement(keep1, 'div', {
      id: 'remove1',
      text: 'Advertisement ',
    });

    addElement(remove1, 'div', {
      id: 'child1',
      text: 'Nested Child',
    });

    addElement(testContainer, 'div', {
      id: 'keep2',
      text: 'Keep Me Too',
    });

    removeExtraneousElementsFrom(testContainer);

    assertEquals(null, document.getElementById('remove1'));
    assertEquals(null, document.getElementById('child1'));
    assertNotEquals(null, document.getElementById('keep1'));
    assertNotEquals(null, document.getElementById('keep2'));

    const textContent = testContainer.textContent || '';
    assertTrue(textContent.includes('Keep Me'));
    assertTrue(textContent.includes('Keep Me Too'));
    assertFalse(textContent.includes('Advertisement'));
    assertFalse(textContent.includes('Nested Child'));
  });

  test('removes advertisement elements correctly', () => {
    addElement(testContainer, 'div', {id: 'remove1', text: 'Advertisement'});
    addElement(testContainer, 'div', {id: 'keep1', text: 'Keep Me'});

    removeExtraneousElementsFrom(testContainer);

    assertEquals(null, document.getElementById('remove1'));
    assertNotEquals(null, document.getElementById('keep1'));
    assertEquals('Keep Me', (testContainer.textContent || '').trim());
  });
});
