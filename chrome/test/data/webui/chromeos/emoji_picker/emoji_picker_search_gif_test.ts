// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EmojiPickerApiProxy, EmojiSearch} from 'chrome://emoji-picker/emoji_picker.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertGT} from 'chrome://webui-test/chai_assert.js';

import {assertEmojiImageAlt, initialiseEmojiPickerForTest, timeout, waitForCondition} from './emoji_picker_test_util.js';
import {TestEmojiPickerApiProxy} from './test_emoji_picker_api_proxy.js';


suite('emoji-search-gif', () => {
  EmojiPickerApiProxy.setInstance(new TestEmojiPickerApiProxy());
  let emojiSearch: EmojiSearch;
  let findInEmojiPicker: (...path: string[]) => HTMLElement | null;
  let scrollDown: ((height: number) => void);
  let scrollToBottom: () => void;
  setup(async () => {
    const newPicker = initialiseEmojiPickerForTest();
    findInEmojiPicker = newPicker.findInEmojiPicker;
    scrollDown = newPicker.scrollDown;
    scrollToBottom = () => {
      const thisRect =
          findInEmojiPicker('emoji-search')!.shadowRoot!.getElementById(
              'results');
      const searchResultRect =
          findInEmojiPicker('emoji-search')!.shadowRoot!.getElementById(
              'search-results');
      if (searchResultRect && thisRect) {
        thisRect.scrollTop += searchResultRect.getBoundingClientRect().bottom;
      }
    };

    await newPicker.readyPromise;

    emojiSearch = findInEmojiPicker('emoji-search') as EmojiSearch;
  });

  test('By default, there is no search result.', () => {
    const searchResults = findInEmojiPicker('emoji-search', '#results');
    assert(!searchResults);
  });

  test(
      'If matching, search should return emoji, emoticon, and GIF results.',
      async () => {
        emojiSearch.setSearchQuery('face');
        await waitForCondition(
            () => findInEmojiPicker(
                'emoji-search', 'emoji-group[category="emoji"]'),
            'wait for search result to render');
        const emojiResults =
            findInEmojiPicker(
                'emoji-search',
                'emoji-group')!.shadowRoot!.querySelectorAll('.emoji-button');
        assertGT(emojiResults.length, 0);
        const emoticonResults =
            findInEmojiPicker(
                'emoji-search', 'emoji-group[category="emoticon"]')!.shadowRoot!
                .querySelectorAll('.emoji-button');
        assertGT(emoticonResults.length, 0);
        const gifResults =
            findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]')!
                .shadowRoot!.querySelectorAll('emoji-image');
        assertGT(gifResults.length, 0);
      });

  test('GIF search results are displayed in the correct order.', async () => {
    emojiSearch.setSearchQuery('face');
    await waitForCondition(
        () => findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]'),
        'wait for search result to render');

    const gifResults =
        findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]')!
            .shadowRoot!.querySelectorAll('emoji-image');
    assertGT(gifResults.length, 0);

    assertEquals(gifResults.length, 6);

    // Check display is correct.
    const leftColResults =
        findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]')!
            .shadowRoot!.querySelectorAll<HTMLImageElement>(
                'div.left-column > emoji-image');
    assertEquals(leftColResults.length, 3);
    assertEmojiImageAlt(leftColResults[0], 'Left 1');
    assertEmojiImageAlt(leftColResults[1], 'Left 2');
    assertEmojiImageAlt(leftColResults[2], 'Left 3');

    const rightColResults =
        findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]')!
            .shadowRoot!.querySelectorAll<HTMLImageElement>(
                'div.right-column > emoji-image');
    assertEquals(rightColResults.length, 3);
    assertEmojiImageAlt(rightColResults[0], 'Right 1');
    assertEmojiImageAlt(rightColResults[1], 'Right 2');
    assertEmojiImageAlt(rightColResults[2], 'Right 3');
  });

  test(
      'User does not load more GIFs if they have not scrolled down far enough',
      async () => {
        emojiSearch.setSearchQuery('face');
        await waitForCondition(
            () => findInEmojiPicker(
                'emoji-search', 'emoji-group[category="gif"]'),
            'wait for search result to render');

        const gifResults1 =
            findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]')!
                .shadowRoot!.querySelectorAll('emoji-image');
        assertGT(gifResults1.length, 0);
        assertEquals(gifResults1.length, 6);

        // Scroll down a little bit to activate checking if we need more GIFs.
        scrollDown(100);

        // Wait for emoji picker to scroll and check if more GIFs need to be
        // appended. Not possible to use waitForCondition here as nothing is
        // actually supposed to change.
        await timeout(400);

        const gifResults2 =
            findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]')!
                .shadowRoot!.querySelectorAll('emoji-image');
        assertEquals(gifResults2.length, 6);
      });

  test('More GIFs are loaded when user scrolls down far enough.', async () => {
    emojiSearch.setSearchQuery('face');
    await waitForCondition(
        () => findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]'),
        'wait for search result to render');

    const gifResults1 =
        findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]')!
            .shadowRoot!.querySelectorAll('emoji-image');
    assertGT(gifResults1.length, 0);
    assertEquals(gifResults1.length, 6);

    scrollToBottom();

    await waitForCondition(
        () => findInEmojiPicker(
                  'emoji-search', 'emoji-group[category="gif"]')!.shadowRoot!
                  .querySelectorAll('emoji-image')
                  .length === 12,
        'wait for scroll and new gifs to render');

    const gifResults2 =
        findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]')!
            .shadowRoot!.querySelectorAll('emoji-image');
    assertEquals(gifResults2.length, 12);
  });

  test('Appended GIFs are displayed in the correct order.', async () => {
    emojiSearch.setSearchQuery('face');
    await waitForCondition(
        () => findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]'),
        'wait for search results to render');

    const gifResults1 =
        findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]')!
            .shadowRoot!.querySelectorAll('emoji-image');
    assertGT(gifResults1.length, 0);
    assertEquals(gifResults1.length, 6);

    scrollToBottom();

    await waitForCondition(
        () => findInEmojiPicker(
                  'emoji-search', 'emoji-group[category="gif"]')!.shadowRoot!
                  .querySelectorAll('emoji-image')
                  .length === 12,
        'wait for scroll and new emoji to render');

    const gifResults2 =
        findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]')!
            .shadowRoot!.querySelectorAll('emoji-image');
    assertEquals(gifResults2.length, 12);

    // Check display is correct.
    const leftColResults =
        findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]')!
            .shadowRoot!.querySelectorAll<HTMLImageElement>(
                'div.left-column > emoji-image');
    assertEquals(leftColResults.length, 6);
    assertEmojiImageAlt(leftColResults[0], 'Left 1');
    assertEmojiImageAlt(leftColResults[1], 'Left 2');
    assertEmojiImageAlt(leftColResults[2], 'Left 3');
    assertEmojiImageAlt(leftColResults[3], 'Left 4');
    assertEmojiImageAlt(leftColResults[4], 'Left 5');
    assertEmojiImageAlt(leftColResults[5], 'Left 6');

    const rightColResults =
        findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]')!
            .shadowRoot!.querySelectorAll<HTMLImageElement>(
                'div.right-column > emoji-image');
    assertEquals(rightColResults.length, 6);
    assertEmojiImageAlt(rightColResults[0], 'Right 1');
    assertEmojiImageAlt(rightColResults[1], 'Right 2');
    assertEmojiImageAlt(rightColResults[2], 'Right 3');
    assertEmojiImageAlt(rightColResults[3], 'Right 4');
    assertEmojiImageAlt(rightColResults[4], 'Right 5');
    assertEmojiImageAlt(rightColResults[5], 'Right 6');
  });

  test('Blank search queries should be prevented in API Proxy', async () => {
    // Given a real API proxy.
    const apiProxy = new EmojiPickerApiProxy();

    // When blank queries are sent.
    const {status, searchGifs} = await apiProxy.searchGifs('   ');

    // Then empty result should be returned.
    assertEquals(status, 0);
    assertEquals(searchGifs.next, '');
    assertEquals(searchGifs.results.length, 0);
  });
});
