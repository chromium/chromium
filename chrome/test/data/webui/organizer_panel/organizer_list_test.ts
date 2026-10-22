// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://organizer-panel.top-chrome/organizer_panel.js';

import type {OrganizerListElement} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {organizerPanelBrowserProxyFactory, OrganizerPanelPageHandlerRemote, SearchApiProxyImpl} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSearchApiProxy} from './test_search_api_proxy.js';
import {TestSectionDelegate} from './test_section_delegate.js';

suite('OrganizerListTest', () => {
  let list: OrganizerListElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const mockHandler = TestMock.fromClass(OrganizerPanelPageHandlerRemote);
    mockHandler.setPromiseResolveFor(
        'getSectionState', {state: {expanded: true, showAll: false}});
    const {instance} =
        organizerPanelBrowserProxyFactory.createForTest(mockHandler);
    organizerPanelBrowserProxyFactory.setInstance(instance);
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
        assertEquals(
            null, list.shadowRoot.querySelector('organizer-zero-state'));
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
        assertEquals(
            null, list.shadowRoot.querySelector('organizer-zero-state'));
        assertFalse(isVisible(dividers[0]!));
        assertTrue(isVisible(dividers[1]!));

        // Search matching Section 2 and Section 3 only.
        list.searchQuery = 'Beta';
        await microtasksFinished();

        assertTrue(sections[0]!.hidden);
        assertFalse(sections[1]!.hidden);
        assertFalse(sections[2]!.hidden);
        assertFalse(list.$.sections.hidden);
        assertEquals(
            null, list.shadowRoot.querySelector('organizer-zero-state'));
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
        const noResults = list.shadowRoot.querySelector('organizer-zero-state');
        assertTrue(!!noResults);
        assertEquals('No results', noResults.textContent.trim());

        // Clear search query.
        list.searchQuery = '';
        await microtasksFinished();

        assertFalse(sections[0]!.hidden);
        assertFalse(sections[1]!.hidden);
        assertFalse(sections[2]!.hidden);
        assertFalse(list.$.sections.hidden);
        assertEquals(
            null, list.shadowRoot.querySelector('organizer-zero-state'));
        assertTrue(isVisible(dividers[0]!));
        assertTrue(isVisible(dividers[1]!));
      });

  test(
      'focusFirstSection and focusLastSection focus section headers',
      async () => {
        list.sectionDelegates = [
          new TestSectionDelegate('Section 1'),
          new TestSectionDelegate('Section 2'),
        ];
        await microtasksFinished();

        const sections = list.getVisibleSections();
        assertEquals(2, sections.length);

        assertTrue(list.focusFirstSection());
        assertEquals(
            sections[0]!.$.header.$.expandButton.$.icon,
            getDeepActiveElement());

        assertTrue(list.focusLastSection(/*focusMenuButtonIfVisible=*/ false));
        assertEquals(
            sections[1]!.$.header.$.expandButton.$.icon,
            getDeepActiveElement());

        assertTrue(list.focusLastSection(/*focusMenuButtonIfVisible=*/ true));
        assertEquals(
            sections[1]!.$.header.$.menuButton, getDeepActiveElement());
      });

  test('arrow keys linearly traverse headers and items', async () => {
    list.sectionDelegates = [
      new TestSectionDelegate(
          'Section 1',
          [
            {title: ['Item 1A']},
            {title: ['Item 1B']},
          ]),
      new TestSectionDelegate(
          'Section 2',
          [
            {title: ['Item 2A']},
          ]),
    ];
    await microtasksFinished();

    const sections = list.getVisibleSections();
    assertEquals(2, sections.length);
    const sec1Items = sections[0]!.getVisibleItems();
    assertEquals(2, sec1Items.length);
    const sec2Items = sections[1]!.getVisibleItems();
    assertEquals(1, sec2Items.length);

    // Focus first section header
    list.focusFirstSection();
    assertEquals(
        sections[0]!.$.header.$.expandButton.$.icon, getDeepActiveElement());

    // Down arrow -> Item 1A
    sections[0]!.$.header.$.expandButton.dispatchEvent(
        new KeyboardEvent('keydown', {
          key: 'ArrowDown',
          bubbles: true,
          composed: true,
        }));
    await microtasksFinished();
    assertEquals(
        sec1Items[0]!.$.crUrlListItem.getFocusableElement(),
        getDeepActiveElement());

    // Down arrow -> Item 1B
    sec1Items[0]!.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'ArrowDown',
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();
    assertEquals(
        sec1Items[1]!.$.crUrlListItem.getFocusableElement(),
        getDeepActiveElement());

    // Down arrow -> Section 2 header
    sec1Items[1]!.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'ArrowDown',
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();
    assertEquals(
        sections[1]!.$.header.$.expandButton.$.icon, getDeepActiveElement());

    // Up arrow from Section 2 header focuses Item 1B (the item directly above
    // it!)
    sections[1]!.$.header.$.expandButton.dispatchEvent(
        new KeyboardEvent('keydown', {
          key: 'ArrowUp',
          bubbles: true,
          composed: true,
        }));
    await microtasksFinished();
    assertEquals(
        sec1Items[1]!.$.crUrlListItem.getFocusableElement(),
        getDeepActiveElement());

    // Up arrow -> Item 1A
    sec1Items[1]!.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'ArrowUp',
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();
    assertEquals(
        sec1Items[0]!.$.crUrlListItem.getFocusableElement(),
        getDeepActiveElement());

    // Up arrow -> Section 1 header
    sec1Items[0]!.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'ArrowUp',
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();
    assertEquals(
        sections[0]!.$.header.$.expandButton.$.icon, getDeepActiveElement());
  });

  test(
      'arrow keys navigate across list even when focus is on inner buttons',
      async () => {
        list.sectionDelegates = [
          new TestSectionDelegate(
              'Section 1',
              [
                {
                  title: ['Item 1A'],
                  hoveredActionButton: {
                    icon: 'cr:close',
                    ariaLabel: 'Close tab',
                  },
                },
                {title: ['Item 1B']},
              ]),
          new TestSectionDelegate(
              'Section 2',
              [
                {title: ['Item 2A']},
              ]),
        ];
        await microtasksFinished();

        const sections = list.getVisibleSections();
        const sec1Items = sections[0]!.getVisibleItems();
        const sec2Items = sections[1]!.getVisibleItems();

        // 1. Focus Section 1 menu button (within section header)
        sections[0]!.$.header.focusMenuButton();
        assertEquals(
            sections[0]!.$.header.$.menuButton, getDeepActiveElement());

        // ArrowDown from menu button moves to Item 1A
        sections[0]!.$.header.$.menuButton.dispatchEvent(
            new KeyboardEvent('keydown', {
              key: 'ArrowDown',
              bubbles: true,
              composed: true,
            }));
        await microtasksFinished();
        assertEquals(
            sec1Items[0]!.$.crUrlListItem.getFocusableElement(),
            getDeepActiveElement());

        // ArrowUp from menu button moves to previous item (wraps to Section 2
        // Item 2A)
        sections[0]!.$.header.focusMenuButton();
        assertEquals(
            sections[0]!.$.header.$.menuButton, getDeepActiveElement());

        sections[0]!.$.header.$.menuButton.dispatchEvent(
            new KeyboardEvent('keydown', {
              key: 'ArrowUp',
              bubbles: true,
              composed: true,
            }));
        await microtasksFinished();
        assertEquals(
            sec2Items[0]!.$.crUrlListItem.getFocusableElement(),
            getDeepActiveElement());

        // 2. Focus Item 1A action button (within section item)
        sec1Items[0]!.focus();
        sec1Items[0]!.focusActionButton();
        const actionBtn = sec1Items[0]!.shadowRoot.querySelector<HTMLElement>(
            '#actionButton')!;
        assertEquals(actionBtn, getDeepActiveElement());

        // ArrowDown from action button moves to Item 1B
        actionBtn.dispatchEvent(new KeyboardEvent('keydown', {
          key: 'ArrowDown',
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();
        assertEquals(
            sec1Items[1]!.$.crUrlListItem.getFocusableElement(),
            getDeepActiveElement());

        // ArrowUp from action button moves to Section 1 header
        sec1Items[0]!.focus();
        sec1Items[0]!.focusActionButton();
        assertEquals(actionBtn, getDeepActiveElement());

        actionBtn.dispatchEvent(new KeyboardEvent('keydown', {
          key: 'ArrowUp',
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();
        assertEquals(
            sections[0]!.$.header.$.expandButton.$.icon,
            getDeepActiveElement());
      });

  test(
      'collapsing a section removes its items from arrow navigation',
      async () => {
        list.sectionDelegates = [
          new TestSectionDelegate('Section 1', [{title: ['Item 1A']}]),
          new TestSectionDelegate('Section 2', [{title: ['Item 2A']}]),
        ];
        await microtasksFinished();

        const sections = list.getVisibleSections();
        sections[0]!.$.header.$.expandButton.click();
        await microtasksFinished();

        list.focusFirstSection();
        assertEquals(
            sections[0]!.$.header.$.expandButton.$.icon,
            getDeepActiveElement());

        // Since Section 1 is collapsed, ArrowDown from Section 1 header goes
        // directly to Section 2 header
        sections[0]!.$.header.$.expandButton.dispatchEvent(
            new KeyboardEvent('keydown', {
              key: 'ArrowDown',
              bubbles: true,
              composed: true,
            }));
        await microtasksFinished();
        assertEquals(
            sections[1]!.$.header.$.expandButton.$.icon,
            getDeepActiveElement());
      });

  test(
      'focus-next-section navigates across sections and fires focus-after-list at the end',
      async () => {
        list.sectionDelegates = [
          new TestSectionDelegate('Section 1'),
          new TestSectionDelegate('Section 2'),
        ];
        await microtasksFinished();

        const sections = list.getVisibleSections();
        list.focusFirstSection();

        // From Section 1, firing focus-next-section focuses Section 2 header
        // and prevents default on the source event.
        const intermediateEvent =
            new KeyboardEvent('keydown', {key: 'Tab', cancelable: true});
        sections[0]!.fire(
            'focus-next-section', {sourceEvent: intermediateEvent});
        await microtasksFinished();
        assertEquals(
            sections[1]!.$.header.$.expandButton.$.icon,
            getDeepActiveElement());
        assertTrue(intermediateEvent.defaultPrevented);

        // From Section 2 (last section), firing focus-next-section fires
        // focus-after-list, forwarding sourceEvent without preventing default.
        const lastSectionEvent =
            new KeyboardEvent('keydown', {key: 'Tab', cancelable: true});
        const whenFocusAfterList = eventToPromise('focus-after-list', list);
        sections[1]!.fire(
            'focus-next-section', {sourceEvent: lastSectionEvent});
        const afterEvent = await whenFocusAfterList as CustomEvent;
        assertEquals(lastSectionEvent, afterEvent.detail.sourceEvent);
        assertFalse(lastSectionEvent.defaultPrevented);
      });

  test(
      'focus-previous-section navigates backwards and fires focus-before-list at the start',
      async () => {
        list.sectionDelegates = [
          new TestSectionDelegate('Section 1'),
          new TestSectionDelegate('Section 2'),
        ];
        await microtasksFinished();

        const sections = list.getVisibleSections();
        sections[1]!.focus();

        // From Section 2, focus-previous-section goes to Section 1 menu button
        // (since expanded)
        sections[1]!.fire('focus-previous-section');
        await microtasksFinished();
        assertEquals(
            sections[0]!.$.header.$.menuButton, getDeepActiveElement());

        // From Section 1 (first section), focus-previous-section fires
        // focus-before-list
        const whenFocusBeforeList = eventToPromise('focus-before-list', list);
        sections[0]!.fire('focus-previous-section');
        await whenFocusBeforeList;
      });

  test('focus-current-section focuses the section header', async () => {
    list.sectionDelegates = [
      new TestSectionDelegate('Section 1', [{title: ['Item 1A']}]),
    ];
    await microtasksFinished();

    const sections = list.getVisibleSections();
    const item = sections[0]!.getVisibleItems()[0]!;
    item.focus();

    item.fire('focus-current-section');
    await microtasksFinished();
    assertEquals(
        sections[0]!.$.header.$.expandButton.$.icon, getDeepActiveElement());
  });
});
