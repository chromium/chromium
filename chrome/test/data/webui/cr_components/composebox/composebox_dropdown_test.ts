// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/strings.m.js';
import 'chrome://resources/cr_components/composebox/composebox_dropdown.js';

import type {ComposeboxDropdownElement} from 'chrome://resources/cr_components/composebox/composebox_dropdown.js';
import {createAutocompleteMatch} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {createAutocompleteResultForTesting, createSuggestTemplateInfo} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {RenderType, SideType, SuggestStyle} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
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
      sequenceId: 7,
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
          suggestTemplate: createSuggestTemplateInfo({
            image: {url: 'https://example.com/image1.png', dominantColor: ''},
          }),
          suggestionGroupId: 101,
        }),
        createAutocompleteMatch({
          contents: 'image match 2',
          suggestTemplate: createSuggestTemplateInfo({
            image: {url: 'https://example.com/image2.png', dominantColor: ''},
          }),
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

    const allMatches =
        dropdown.shadowRoot.querySelectorAll('cr-composebox-match');
    assertEquals(3, allMatches.length);
    for (const matchElement of allMatches) {
      assertEquals(7, matchElement.resultSequenceId);
    }
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

  test('tracks isImageBatchLoading_ for rich image suggestions', async () => {
    const createdImages: HTMLImageElement[] = [];
    const OriginalImage = window.Image;
    window.Image = class extends OriginalImage {
      constructor(width?: number, height?: number) {
        super(width, height);
        createdImages.push(this);
      }

      override set src(_value: string) {
        // Prevent real network/IPC fetch in unit test so onload/onerror is
        // controlled deterministically by the test.
      }
    };

    try {
      dropdown.richImageSuggestionsEnabled = true;

      // Hidden rich image matches (e.g. beyond maxSuggestions) should not
      // trigger the ghost loader when no visible rich image matches exist.
      dropdown.maxSuggestions = 1;
      dropdown.result = createAutocompleteResultForTesting({
        matches: [
          createAutocompleteMatch({
            contents: 'visible text match',
          }),
          createAutocompleteMatch({
            contents: 'hidden image match',
            suggestStyle: SuggestStyle.kRichImage,
            suggestTemplate: createSuggestTemplateInfo({
              image: {url: 'https://example.com/hidden.png', dominantColor: ''},
            }),
          }),
        ],
      });
      await microtasksFinished();
      assertFalse(dropdown.isImageBatchLoading_);
      assertEquals(0, createdImages.length);

      // Showing rich image matches starts batch preloading.
      dropdown.maxSuggestions = null;
      dropdown.result = createAutocompleteResultForTesting({
        matches: [
          createAutocompleteMatch({
            contents: 'image match 1',
            suggestStyle: SuggestStyle.kRichImage,
            suggestTemplate: createSuggestTemplateInfo({
              image: {url: 'https://example.com/image1.png', dominantColor: ''},
            }),
          }),
          createAutocompleteMatch({
            contents: 'image match 2',
            suggestStyle: SuggestStyle.kRichImage,
            suggestTemplate: createSuggestTemplateInfo({
              image: {url: 'https://example.com/image2.png', dominantColor: ''},
            }),
          }),
          createAutocompleteMatch({
            contents: 'image match 3 without image',
            suggestStyle: SuggestStyle.kRichImage,
          }),
        ],
      });
      await microtasksFinished();

      const matches =
          dropdown.shadowRoot.querySelectorAll('cr-composebox-match');
      assertEquals(3, matches.length);
      assertEquals(2, createdImages.length);
      assertTrue(dropdown.isImageBatchLoading_);
      assertTrue(matches[0]!.loading);
      assertTrue(matches[1]!.loading);
      assertFalse(matches[2]!.loading);
      assertEquals(-1, matches[0]!.tabIndex);
      assertEquals(-1, matches[1]!.tabIndex);
      assertEquals(0, matches[2]!.tabIndex);

      // Finishing only the first image keeps the batch in loading state.
      createdImages[0]!.onload!(new Event('load'));
      await microtasksFinished();
      assertTrue(dropdown.isImageBatchLoading_);
      assertTrue(matches[0]!.loading);
      assertTrue(matches[1]!.loading);

      // Finishing the remaining image reveals the entire batch atomically.
      createdImages[1]!.onload!(new Event('load'));
      await microtasksFinished();
      assertFalse(dropdown.isImageBatchLoading_);
      assertFalse(matches[0]!.loading);
      assertFalse(matches[1]!.loading);
      assertFalse(matches[2]!.loading);
      assertEquals(0, matches[0]!.tabIndex);
      assertEquals(0, matches[1]!.tabIndex);
      assertEquals(0, matches[2]!.tabIndex);

      // Subsequent result updates with already-loaded image URLs do not
      // re-trigger the ghost loader.
      dropdown.result = createAutocompleteResultForTesting({
        matches: [
          createAutocompleteMatch({
            contents: 'image match 1 updated',
            suggestStyle: SuggestStyle.kRichImage,
            suggestTemplate: createSuggestTemplateInfo({
              image: {url: 'https://example.com/image1.png', dominantColor: ''},
            }),
          }),
          createAutocompleteMatch({
            contents: 'image match 2 updated',
            suggestStyle: SuggestStyle.kRichImage,
            suggestTemplate: createSuggestTemplateInfo({
              image: {url: 'https://example.com/image2.png', dominantColor: ''},
            }),
          }),
        ],
      });
      await microtasksFinished();
      assertFalse(dropdown.isImageBatchLoading_);
      assertEquals(2, createdImages.length);

      // If an image fails to load (onerror), the batch still finishes loading,
      // but the failed URL is not cached as loaded so a subsequent update
      // retries preloading it.
      dropdown.result = createAutocompleteResultForTesting({
        matches: [
          createAutocompleteMatch({
            contents: 'failed image match',
            suggestStyle: SuggestStyle.kRichImage,
            suggestTemplate: createSuggestTemplateInfo({
              image: {url: 'https://example.com/error.png', dominantColor: ''},
            }),
          }),
        ],
      });
      await microtasksFinished();
      assertTrue(dropdown.isImageBatchLoading_);
      assertEquals(3, createdImages.length);

      createdImages[2]!.onerror!(new Event('error'));
      await microtasksFinished();
      assertFalse(dropdown.isImageBatchLoading_);

      dropdown.result = createAutocompleteResultForTesting({
        matches: [
          createAutocompleteMatch({
            contents: 'failed image match retry',
            suggestStyle: SuggestStyle.kRichImage,
            suggestTemplate: createSuggestTemplateInfo({
              image: {url: 'https://example.com/error.png', dominantColor: ''},
            }),
          }),
        ],
      });
      await microtasksFinished();
      assertTrue(dropdown.isImageBatchLoading_);
      assertEquals(4, createdImages.length);
    } finally {
      window.Image = OriginalImage;
    }
  });

  test(
      'getFirstVisibleIndex and getLastVisibleIndex for zero state',
      async () => {
        dropdown.richImageSuggestionsEnabled = true;
        dropdown.result = createAutocompleteResultForTesting({
          input: '',
          matches: [
            createAutocompleteMatch({contents: 'image match 1'}),
            createAutocompleteMatch({contents: 'image match 2'}),
          ],
        });
        await microtasksFinished();

        assertEquals(0, dropdown.getFirstVisibleIndex());
        assertEquals(1, dropdown.getLastVisibleIndex());
      });

  test(
      'getFirstVisibleIndex hides verbatim for typed suggestions', async () => {
        dropdown.richImageSuggestionsEnabled = true;
        dropdown.result = createAutocompleteResultForTesting({
          input: 'test',
          matches: [
            createAutocompleteMatch(
                {contents: 'test', allowedToBeDefaultMatch: true}),
            createAutocompleteMatch({contents: 'suggestion 1'}),
            createAutocompleteMatch({contents: 'suggestion 2'}),
          ],
        });
        await microtasksFinished();

        assertEquals(1, dropdown.getFirstVisibleIndex());
        assertEquals(2, dropdown.getLastVisibleIndex());
      });

  test('getFirstVisibleIndex returns -1 for empty or null result', async () => {
    dropdown.result = null;
    await microtasksFinished();
    assertEquals(-1, dropdown.getFirstVisibleIndex());

    dropdown.result = createAutocompleteResultForTesting({
      matches: [],
    });
    await microtasksFinished();
    assertEquals(-1, dropdown.getFirstVisibleIndex());
  });

  test(
      'unselectOnTabExit unselects when navigating outside bounds',
      async () => {
        dropdown.result = createAutocompleteResultForTesting({
          input: '',
          matches: [
            createAutocompleteMatch({supportsDeletion: false}),
            createAutocompleteMatch({supportsDeletion: false}),
          ],
        });
        await microtasksFinished();

        // Shift-Tab on first visible match unselects.
        dropdown.selectedMatchIndex = 0;
        dropdown.unselectOnTabExit(
            new KeyboardEvent('keydown', {key: 'Tab', shiftKey: true}));
        assertEquals(-1, dropdown.selectedMatchIndex);

        // Forward Tab on last visible match unselects.
        dropdown.selectedMatchIndex = 1;
        dropdown.unselectOnTabExit(new KeyboardEvent('keydown', {key: 'Tab'}));
        assertEquals(-1, dropdown.selectedMatchIndex);

        // Shift-Tab on last visible match does not unselect.
        dropdown.selectedMatchIndex = 1;
        dropdown.unselectOnTabExit(
            new KeyboardEvent('keydown', {key: 'Tab', shiftKey: true}));
        assertEquals(1, dropdown.selectedMatchIndex);

        // When matches support deletion, Tab/Shift-Tab accounts for the inner
        // remove button focus state.
        dropdown.result = createAutocompleteResultForTesting({
          input: '',
          matches: [
            createAutocompleteMatch({supportsDeletion: true}),
            createAutocompleteMatch({supportsDeletion: true}),
          ],
        });
        dropdown.selectedMatchIndex = 0;
        await microtasksFinished();

        const matches =
            dropdown.shadowRoot.querySelectorAll('cr-composebox-match');
        matches[0]!.$.remove.style.display = 'inline-flex';
        matches[0]!.$.remove.focus();
        await microtasksFinished();

        // Shift-Tab while focused on the first match's remove button moves
        // focus to the match row rather than exiting the dropdown.
        dropdown.unselectOnTabExit(
            new KeyboardEvent('keydown', {key: 'Tab', shiftKey: true}));
        assertEquals(0, dropdown.selectedMatchIndex);

        // Forward Tab on the last match row moves focus to its remove button
        // rather than exiting the dropdown.
        dropdown.selectedMatchIndex = 1;
        await microtasksFinished();
        matches[1]!.focus();
        await microtasksFinished();
        dropdown.unselectOnTabExit(new KeyboardEvent('keydown', {key: 'Tab'}));
        assertEquals(1, dropdown.selectedMatchIndex);

        // Forward Tab while focused on the last match's remove button exits
        // the dropdown and unselects.
        matches[1]!.$.remove.style.display = 'inline-flex';
        matches[1]!.$.remove.focus();
        await microtasksFinished();
        dropdown.unselectOnTabExit(new KeyboardEvent('keydown', {key: 'Tab'}));
        assertEquals(-1, dropdown.selectedMatchIndex);
      });
});
