// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://organizer-panel.top-chrome/organizer_panel.js';

import type {OrganizerListSectionItemDescriptionElement} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

const TEST_DESC_1 = 'google.com';
const TEST_DESC_2 = '5 mins ago';
const TEST_DESC_3 = 'Work Group';
const TEST_PREFIX_ID = 'customPrefixDot';

suite('OrganizerListSectionItemDescriptionTest', () => {
  let element: OrganizerListSectionItemDescriptionElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('organizer-list-section-item-description');
    document.body.appendChild(element);
    await microtasksFinished();
  });

  test('renders single description without separator', async () => {
    element.descriptionParts = [{text: TEST_DESC_1}];
    await microtasksFinished();

    const descriptionParts =
        element.$.descriptionParts.querySelectorAll<HTMLElement>(
            '.description-part');
    const separators =
        element.$.descriptionParts.querySelectorAll<HTMLElement>('.separator');
    assertEquals(1, descriptionParts.length);
    assertEquals(0, separators.length);
    assertEquals(TEST_DESC_1, descriptionParts[0]!.textContent.trim());
    assertEquals(TEST_DESC_1, descriptionParts[0]!.getAttribute('title'));
  });

  test('renders multiple descriptions separated by dot', async () => {
    element.descriptionParts = [
      {text: TEST_DESC_3},
      {text: TEST_DESC_1, elideFromStart: true},
      {text: TEST_DESC_2},
    ];
    await microtasksFinished();

    const descriptionParts =
        element.$.descriptionParts.querySelectorAll<HTMLElement>(
            '.description-part');
    const separators =
        element.$.descriptionParts.querySelectorAll<HTMLElement>('.separator');
    assertEquals(3, descriptionParts.length);
    assertEquals(2, separators.length);
    assertEquals(TEST_DESC_3, descriptionParts[0]!.textContent.trim());
    assertEquals(TEST_DESC_1, descriptionParts[1]!.textContent.trim());
    assertEquals(TEST_DESC_2, descriptionParts[2]!.textContent.trim());
    assertEquals('•', separators[0]!.textContent.trim());
    assertEquals('•', separators[1]!.textContent.trim());
  });

  test('applies elideFromStart class when configured', async () => {
    element.descriptionParts = [
      {text: TEST_DESC_1, elideFromStart: true},
      {text: TEST_DESC_2, elideFromStart: false},
    ];
    await microtasksFinished();

    const descriptionTexts =
        element.$.descriptionParts.querySelectorAll<HTMLElement>(
            '.description-text');
    assertEquals(2, descriptionTexts.length);
    assertTrue(descriptionTexts[0]!.classList.contains('elide-from-start'));
    assertFalse(descriptionTexts[1]!.classList.contains('elide-from-start'));
  });

  test('renders prefixElement when provided', async () => {
    element.descriptionParts = [
      {
        text: TEST_DESC_3,
        prefixElement: html`<span id="${TEST_PREFIX_ID}">●</span>`,
      },
      {text: TEST_DESC_1},
    ];
    await microtasksFinished();

    const descriptionParts =
        element.$.descriptionParts.querySelectorAll<HTMLElement>(
            '.description-part');
    assertEquals(2, descriptionParts.length);

    const prefix = descriptionParts[0]!.querySelector(`#${TEST_PREFIX_ID}`);
    assertTrue(!!prefix);
    assertEquals('●', prefix.textContent);
    assertEquals(null, descriptionParts[1]!.querySelector('.prefix'));
  });

  test('renders highlights when highlight ranges are provided', async () => {
    element.descriptionParts = [{text: TEST_DESC_1}];
    element.highlightRanges = [[{start: 0, length: 6}]];
    await microtasksFinished();

    const descriptionParts =
        element.$.descriptionParts.querySelectorAll<HTMLElement>(
            '.description-part');
    assertEquals(1, descriptionParts.length);

    const hits = descriptionParts[0]!.querySelectorAll<HTMLElement>('b');
    assertEquals(1, hits.length);
    assertEquals('google', hits[0]!.textContent);
    assertEquals('700', getComputedStyle(hits[0]!).fontWeight);
  });

  test(
      'renders highlights across multiple parts preserving elideFromStart',
      async () => {
        element.descriptionParts = [
          {text: TEST_DESC_1, elideFromStart: true},
          {text: TEST_DESC_2},
        ];
        element.highlightRanges = [
          [{start: 0, length: 6}],
          [{start: 0, length: 1}],
        ];
        await microtasksFinished();

        const descriptionTexts =
            element.$.descriptionParts.querySelectorAll<HTMLElement>(
                '.description-text');
        assertEquals(2, descriptionTexts.length);
        assertTrue(descriptionTexts[0]!.classList.contains('elide-from-start'));

        const hitsPart1 =
            descriptionTexts[0]!.querySelectorAll<HTMLElement>('b');
        assertEquals(1, hitsPart1.length);
        assertEquals('google', hitsPart1[0]!.textContent);
        assertEquals('700', getComputedStyle(hitsPart1[0]!).fontWeight);

        const hitsPart2 =
            descriptionTexts[1]!.querySelectorAll<HTMLElement>('b');
        assertEquals(1, hitsPart2.length);
        assertEquals('5', hitsPart2[0]!.textContent);
        assertEquals('700', getComputedStyle(hitsPart2[0]!).fontWeight);
      });

  test('renders highlights when prefixElement is present', async () => {
    element.descriptionParts = [
      {
        text: TEST_DESC_3,
        prefixElement: html`<span id="${TEST_PREFIX_ID}">●</span>`,
      },
    ];
    element.highlightRanges = [[{start: 0, length: 4}]];
    await microtasksFinished();

    const descriptionParts =
        element.$.descriptionParts.querySelectorAll<HTMLElement>(
            '.description-part');
    assertEquals(1, descriptionParts.length);

    const prefix = descriptionParts[0]!.querySelector(`#${TEST_PREFIX_ID}`);
    assertTrue(!!prefix);
    assertEquals('●', prefix.textContent);

    const hits = descriptionParts[0]!.querySelectorAll<HTMLElement>('b');
    assertEquals(1, hits.length);
    assertEquals('Work', hits[0]!.textContent);
    assertEquals('700', getComputedStyle(hits[0]!).fontWeight);
  });

  test('clears highlights when highlight ranges are emptied', async () => {
    element.descriptionParts = [{text: TEST_DESC_1}];
    element.highlightRanges = [[{start: 0, length: 6}]];
    await microtasksFinished();

    assertEquals(
        1,
        element.$.descriptionParts.querySelectorAll<HTMLElement>('b').length);

    element.highlightRanges = [];
    await microtasksFinished();

    assertEquals(
        0,
        element.$.descriptionParts.querySelectorAll<HTMLElement>('b').length);
    assertEquals(
        TEST_DESC_1,
        element.$.descriptionParts
            .querySelector<HTMLElement>(
                '.description-text bdi')!.textContent.trim());
  });
});
