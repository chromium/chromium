// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/strings.m.js';
import 'chrome://resources/cr_components/composebox/composebox_dropdown.js';

import type {ComposeboxDropdownElement} from 'chrome://resources/cr_components/composebox/composebox_dropdown.js';
import {createAutocompleteMatch} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {createAutocompleteResultForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {RenderType, SideType} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('ComposeboxDropdown', () => {
  let dropdown: ComposeboxDropdownElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dropdown = document.createElement('cr-composebox-dropdown');
    document.body.appendChild(dropdown);
    await microtasksFinished();
  });

  test('flat fallback list when rich image suggestions disabled', async () => {
    dropdown.richImageSuggestionsEnabled = false;
    dropdown.result = createAutocompleteResultForTesting({
      matches: [
        createAutocompleteMatch({contents: 'match 1'}),
        createAutocompleteMatch({contents: 'match 2'}),
      ],
    });
    await microtasksFinished();

    const headers = dropdown.shadowRoot.querySelectorAll('.header');
    assertEquals(0, headers.length);

    const gridContainers =
        dropdown.shadowRoot.querySelectorAll('.matches.grid');
    assertEquals(0, gridContainers.length);

    const matches = dropdown.shadowRoot.querySelectorAll('cr-composebox-match');
    assertEquals(2, matches.length);
  });

  test('renders groups and headers when flag enabled', async () => {
    dropdown.richImageSuggestionsEnabled = true;
    dropdown.result = createAutocompleteResultForTesting({
      suggestionGroupsMap: {
        100: {
          header: 'Recent searches',
          renderType: RenderType.kDefaultVertical,
          sideType: SideType.kDefaultPrimary,
        },
        101: {
          header: 'Create with AI',
          renderType: RenderType.kGrid,
          sideType: SideType.kDefaultPrimary,
        },
      },
      matches: [
        createAutocompleteMatch({
          contents: 'text match',
          suggestionGroupId: 100,
        }),
        createAutocompleteMatch({
          contents: 'image match 1',
          imageUrl: 'https://example.com/image1.png',
          suggestionGroupId: 101,
        }),
        createAutocompleteMatch({
          contents: 'image match 2',
          imageUrl: 'https://example.com/image2.png',
          suggestionGroupId: 101,
        }),
      ],
    });
    await microtasksFinished();

    const headers = dropdown.shadowRoot.querySelectorAll('.header');
    assertEquals(2, headers.length);
    assertEquals('Recent searches', headers[0]!.textContent.trim());
    assertEquals('header_100', headers[0]!.id);
    assertEquals('Create with AI', headers[1]!.textContent.trim());
    assertEquals('header_101', headers[1]!.id);

    const verticalMatchesContainer =
        dropdown.shadowRoot.querySelector('.matches.vertical');
    assertTrue(!!verticalMatchesContainer);
    const verticalMatches =
        verticalMatchesContainer.querySelectorAll('cr-composebox-match');
    assertEquals(1, verticalMatches.length);

    const gridMatchesContainer =
        dropdown.shadowRoot.querySelector('.matches.grid');
    assertTrue(!!gridMatchesContainer);
    const gridMatches =
        gridMatchesContainer.querySelectorAll('cr-composebox-match');
    assertEquals(2, gridMatches.length);
  });


  test('header mousedown prevents default', async () => {
    dropdown.richImageSuggestionsEnabled = true;
    dropdown.result = createAutocompleteResultForTesting({
      suggestionGroupsMap: {
        100: {
          header: 'Header Title',
          renderType: RenderType.kDefaultVertical,
          sideType: SideType.kDefaultPrimary,
        },
      },
      matches: [
        createAutocompleteMatch({contents: 'match', suggestionGroupId: 100}),
      ],
    });
    await microtasksFinished();

    const header = dropdown.shadowRoot.querySelector<HTMLElement>('.header');
    assertTrue(!!header);

    const event = new MouseEvent('mousedown', {cancelable: true});
    header.dispatchEvent(event);
    assertTrue(event.defaultPrevented);
  });

  test('selection navigation works across groups', async () => {
    dropdown.richImageSuggestionsEnabled = true;
    dropdown.result = createAutocompleteResultForTesting({
      suggestionGroupsMap: {
        100: {
          header: 'Group 1',
          renderType: RenderType.kDefaultVertical,
          sideType: SideType.kDefaultPrimary,
        },
        101: {
          header: 'Group 2',
          renderType: RenderType.kGrid,
          sideType: SideType.kDefaultPrimary,
        },
      },
      matches: [
        createAutocompleteMatch({contents: 'm0', suggestionGroupId: 100}),
        createAutocompleteMatch({contents: 'm1', suggestionGroupId: 101}),
      ],
    });
    await microtasksFinished();

    dropdown.selectFirst();
    assertEquals(0, dropdown.selectedMatchIndex);

    dropdown.selectNext();
    assertEquals(1, dropdown.selectedMatchIndex);

    dropdown.selectPrevious();
    assertEquals(0, dropdown.selectedMatchIndex);

    dropdown.unselect();
    assertEquals(-1, dropdown.selectedMatchIndex);
  });

  test(
      'scrolls selected match into view when selectedMatchIndex changes',
      async () => {
        dropdown.richImageSuggestionsEnabled = true;
        dropdown.result = createAutocompleteResultForTesting({
          suggestionGroupsMap: {
            101: {
              header: 'Create with AI',
              renderType: RenderType.kGrid,
              sideType: SideType.kDefaultPrimary,
            },
          },
          matches: [
            createAutocompleteMatch({
              contents: 'image match 1',
              suggestionGroupId: 101,
            }),
            createAutocompleteMatch({
              contents: 'image match 2',
              suggestionGroupId: 101,
            }),
          ],
        });
        await microtasksFinished();

        const match1 =
            dropdown.shadowRoot.querySelector<HTMLElement>('#match1')!;
        let scrollCalled = false;
        match1.scrollIntoView = (options?: ScrollIntoViewOptions) => {
          scrollCalled = true;
          assertEquals('nearest', options?.block);
          assertEquals('nearest', options?.inline);
        };

        dropdown.selectedMatchIndex = 1;
        await microtasksFinished();

        assertTrue(scrollCalled);
      });
});
