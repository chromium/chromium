// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EmojiPicker} from 'chrome://emoji-picker/emoji_picker.js';
import {EmojiPickerApiProxyImpl} from 'chrome://emoji-picker/emoji_picker_api_proxy.js';
import {EMOJI_PICKER_READY} from 'chrome://emoji-picker/events.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertGT} from 'chrome://webui-test/chromeos/chai_assert.js';

import {deepQuerySelector, timeout, waitForCondition} from './emoji_picker_test_util.js';
import {TestEmojiPickerApiProxyImpl} from './test_emoji_picker_api_proxy.js';


suite('emoji-search-gif', () => {
  /** @type {!EmojiPicker} */
  let emojiPicker;
  /** @type {function(...!string): ?HTMLElement} */
  let findInEmojiPicker;
  let emojiSearch;
  /** @type {function(string): void} */
  let scrollDown;
  /** @type {function(): void} */
  let scrollToBottom;
  setup(() => {
    // Reset DOM state.
    document.body.innerHTML = '';
    window.localStorage.clear();

    EmojiPickerApiProxyImpl.setInstance(new TestEmojiPickerApiProxyImpl());

    // Set default incognito state to False.
    EmojiPickerApiProxyImpl.getInstance().isIncognitoTextField = () =>
        new Promise((resolve) => resolve({incognito: false}));

    EmojiPicker.configs = () => ({
      'dataUrls': {
        'emoji': [
          '/emoji_test_ordering_start.json',
          '/emoji_test_ordering_remaining.json',
        ],
        'emoticon': ['/emoticon_test_ordering.json'],
        'symbol': ['/symbol_test_ordering.json'],
        'gif': [],
      },
    });

    emojiPicker =
        /** @type {!EmojiPicker} */ (document.createElement('emoji-picker'));

    findInEmojiPicker = (...path) => deepQuerySelector(emojiPicker, path);

    scrollDown = (height) => {
      const thisRect = findInEmojiPicker('emoji-search')
                           .shadowRoot.getElementById('results');
      if (thisRect) {
        thisRect.scrollTop += height;
      }
    };

    scrollToBottom = () => {
      const thisRect = findInEmojiPicker('emoji-search')
                           .shadowRoot.getElementById('results');
      const searchResultRect = findInEmojiPicker('emoji-search')
                                   .shadowRoot.getElementById('search-results');
      if (searchResultRect && thisRect) {
        thisRect.scrollTop += searchResultRect.getBoundingClientRect().bottom;
      }
    };

    // Wait until emoji data is loaded before executing tests.
    return new Promise((resolve) => {
      emojiPicker.addEventListener(EMOJI_PICKER_READY, () => {
        flush();
        resolve();
      });
      document.body.appendChild(emojiPicker);
      emojiSearch = findInEmojiPicker('emoji-search');
    });
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
                'emoji-search', 'emoji-group[category="emoji"]'));
        const emojiResults = findInEmojiPicker('emoji-search', 'emoji-group')
                                 .shadowRoot.querySelectorAll('.emoji-button');
        assertGT(emojiResults.length, 0);
        const emoticonResults =
            findInEmojiPicker(
                'emoji-search', 'emoji-group[category="emoticon"]')
                .shadowRoot.querySelectorAll('.emoji-button');
        assertGT(emoticonResults.length, 0);
        const gifResults =
            findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]')
                .shadowRoot.querySelectorAll('.emoji-button');
        assertGT(gifResults.length, 0);
      });

  test('GIF search results are displayed in the correct order.', async () => {
    emojiSearch.setSearchQuery('face');
    await waitForCondition(
        () => findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]'));

    const gifResults =
        findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]')
            .shadowRoot.querySelectorAll('.emoji-button');
    assertGT(gifResults.length, 0);

    assertEquals(gifResults.length, 6);

    // Check display is correct.
    const leftColResults =
        findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]')
            .shadowRoot.querySelectorAll('div.left-column > .emoji-button');
    assertEquals(leftColResults.length, 3);
    assert(leftColResults[0].alt === 'Left 1');
    assert(leftColResults[1].alt === 'Left 2');
    assert(leftColResults[2].alt === 'Left 3');

    const rightColResults =
        findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]')
            .shadowRoot.querySelectorAll('div.right-column > .emoji-button');
    assertEquals(rightColResults.length, 3);
    assert(rightColResults[0].alt === 'Right 1');
    assert(rightColResults[1].alt === 'Right 2');
    assert(rightColResults[2].alt === 'Right 3');
  });

  test(
      'User does not load more GIFs if they have not scrolled down far enough',
      async () => {
        emojiSearch.setSearchQuery('face');
        await waitForCondition(
            () => findInEmojiPicker(
                'emoji-search', 'emoji-group[category="gif"]'));

        const gifResults1 =
            findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]')
                .shadowRoot.querySelectorAll('.emoji-button');
        assertGT(gifResults1.length, 0);
        assertEquals(gifResults1.length, 6);

        // Scroll down a little bit to activate checking if we need more GIFs.
        scrollDown(100);

        // Wait for emoji picker to scroll and check if more GIFs need to be
        // appended. Not possible to use waitForCondition here as nothing is
        // actually supposed to change.
        await timeout(400);

        const gifResults2 =
            findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]')
                .shadowRoot.querySelectorAll('.emoji-button');
        assertEquals(gifResults2.length, 6);
      });

  test('More GIFs are loaded when user scrolls down far enough.', async () => {
    emojiSearch.setSearchQuery('face');
    await waitForCondition(
        () => findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]'));

    const gifResults1 =
        findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]')
            .shadowRoot.querySelectorAll('.emoji-button');
    assertGT(gifResults1.length, 0);
    assertEquals(gifResults1.length, 6);

    scrollToBottom();

    // Wait for emoji picker to scroll and render new GIFs.
    await waitForCondition(
        () => findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]')
                  .shadowRoot.querySelectorAll('.emoji-button')
                  .length === 12);

    const gifResults2 =
        findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]')
            .shadowRoot.querySelectorAll('.emoji-button');
    assertEquals(gifResults2.length, 12);
  });

  test('Appended GIFs are displayed in the correct order.', async () => {
    emojiSearch.setSearchQuery('face');
    await waitForCondition(
        () => findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]'));

    const gifResults1 =
        findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]')
            .shadowRoot.querySelectorAll('.emoji-button');
    assertGT(gifResults1.length, 0);
    assertEquals(gifResults1.length, 6);

    scrollToBottom();

    // Wait for emoji picker to scroll and render new GIFs.
    await waitForCondition(
        () => findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]')
                  .shadowRoot.querySelectorAll('.emoji-button')
                  .length === 12);

    const gifResults2 =
        findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]')
            .shadowRoot.querySelectorAll('.emoji-button');
    assertEquals(gifResults2.length, 12);

    // Check display is correct.
    const leftColResults =
        findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]')
            .shadowRoot.querySelectorAll('div.left-column > .emoji-button');
    assertEquals(leftColResults.length, 6);
    assert(leftColResults[0].alt === 'Left 1');
    assert(leftColResults[1].alt === 'Left 2');
    assert(leftColResults[2].alt === 'Left 3');
    assert(leftColResults[3].alt === 'Left 4');
    assert(leftColResults[4].alt === 'Left 5');
    assert(leftColResults[5].alt === 'Left 6');

    const rightColResults =
        findInEmojiPicker('emoji-search', 'emoji-group[category="gif"]')
            .shadowRoot.querySelectorAll('div.right-column > .emoji-button');
    assertEquals(rightColResults.length, 6);
    assert(rightColResults[0].alt === 'Right 1');
    assert(rightColResults[1].alt === 'Right 2');
    assert(rightColResults[2].alt === 'Right 3');
    assert(rightColResults[3].alt === 'Right 4');
    assert(rightColResults[4].alt === 'Right 5');
    assert(rightColResults[5].alt === 'Right 6');
  });
});
