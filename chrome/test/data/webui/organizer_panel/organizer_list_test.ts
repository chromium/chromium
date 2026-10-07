// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://organizer-panel.top-chrome/organizer_panel.js';

import type {OrganizerListElement} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {SearchApiProxyImpl} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSearchApiProxy} from './test_search_api_proxy.js';
import {TestSectionDelegate} from './test_section_delegate.js';

suite('OrganizerListTest', () => {
  let list: OrganizerListElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    SearchApiProxyImpl.setInstance(new TestSearchApiProxy());
    list = document.createElement('organizer-list');
    document.body.appendChild(list);
    await microtasksFinished();
  });

  test('renders sections for delegates', async () => {
    list.sectionDelegates = [
      new TestSectionDelegate('Section 1'),
    ];
    await microtasksFinished();

    const sections = list.shadowRoot.querySelectorAll('organizer-list-section');
    assertEquals(1, sections.length);
    assertTrue(!!sections[0]!.delegate);
    assertEquals('Section 1', sections[0]!.delegate.getHeader());

    const dividers = list.shadowRoot.querySelectorAll('.divider');
    assertEquals(0, dividers.length);
  });

  test('renders dividers between sections', async () => {
    list.sectionDelegates = [
      new TestSectionDelegate('Section 1'),
      new TestSectionDelegate('Section 2'),
      new TestSectionDelegate('Section 3'),
    ];
    await microtasksFinished();

    const sections = list.shadowRoot.querySelectorAll('organizer-list-section');
    assertEquals(3, sections.length);

    const dividers = list.shadowRoot.querySelectorAll('.divider');
    assertEquals(2, dividers.length);
    assertTrue(isVisible(dividers[0]!));
    assertTrue(isVisible(dividers[1]!));
  });

  test(
      'hides sections without matches and shows no results when all hidden',
      async () => {
        list.sectionDelegates = [
          new TestSectionDelegate('Section 1', [{title: ['Alpha']}]),
          new TestSectionDelegate('Section 2', [{title: ['Beta']}]),
          new TestSectionDelegate('Section 3', [{title: ['Alpha Beta']}]),
        ];
        await microtasksFinished();

        const sections =
            list.shadowRoot.querySelectorAll('organizer-list-section');
        assertEquals(3, sections.length);
        assertFalse(list.$.sections.hidden);
        assertEquals(null, list.shadowRoot.querySelector('#noResults'));
        const dividers = list.shadowRoot.querySelectorAll('.divider');
        assertEquals(2, dividers.length);
        assertTrue(isVisible(dividers[0]!));
        assertTrue(isVisible(dividers[1]!));

        // Search matching Section 1 and Section 3 only.
        list.searchQuery = 'Alpha';
        await microtasksFinished();

        assertFalse(sections[0]!.hidden);
        assertTrue(sections[1]!.hidden);
        assertFalse(sections[2]!.hidden);
        assertFalse(list.$.sections.hidden);
        assertEquals(null, list.shadowRoot.querySelector('#noResults'));
        assertFalse(isVisible(dividers[0]!));
        assertTrue(isVisible(dividers[1]!));

        // Search matching Section 2 and Section 3 only.
        list.searchQuery = 'Beta';
        await microtasksFinished();

        assertTrue(sections[0]!.hidden);
        assertFalse(sections[1]!.hidden);
        assertFalse(sections[2]!.hidden);
        assertFalse(list.$.sections.hidden);
        assertEquals(null, list.shadowRoot.querySelector('#noResults'));
        assertFalse(isVisible(dividers[0]!));
        assertTrue(isVisible(dividers[1]!));

        // Search matching no sections.
        list.searchQuery = 'Gamma';
        await microtasksFinished();

        assertTrue(sections[0]!.hidden);
        assertTrue(sections[1]!.hidden);
        assertTrue(sections[2]!.hidden);
        assertTrue(list.$.sections.hidden);
        assertFalse(isVisible(dividers[0]!));
        assertFalse(isVisible(dividers[1]!));
        const noResults = list.shadowRoot.querySelector('#noResults');
        assertTrue(!!noResults);
        assertEquals('No results', noResults.textContent.trim());

        // Clear search query.
        list.searchQuery = '';
        await microtasksFinished();

        assertFalse(sections[0]!.hidden);
        assertFalse(sections[1]!.hidden);
        assertFalse(sections[2]!.hidden);
        assertFalse(list.$.sections.hidden);
        assertEquals(null, list.shadowRoot.querySelector('#noResults'));
        assertTrue(isVisible(dividers[0]!));
        assertTrue(isVisible(dividers[1]!));
      });
});
