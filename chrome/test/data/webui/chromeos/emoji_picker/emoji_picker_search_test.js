// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EmojiPicker} from 'chrome://emoji-picker/emoji_picker.js';
import {EMOJI_BUTTON_CLICK, EMOJI_PICKER_READY} from 'chrome://emoji-picker/events.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertGT} from '../../chai_assert.js';

import {deepQuerySelector, waitWithTimeout} from './emoji_picker_test_util.js';

const ACTIVE_CATEGORY_BUTTON = 'category-button-active';

function isCategoryButtonActive(element) {
  assert(element, 'category button element should not be null.');
  return element.classList.contains(ACTIVE_CATEGORY_BUTTON);
}

suite('emoji-search', () => {
  /** @type {!EmojiPicker} */
  let emojiPicker;
  /** @type {function(...!string): ?HTMLElement} */
  let findInEmojiPicker;
  let emojiSearch;
  setup(() => {
    // Reset DOM state.
    document.body.innerHTML = '';
    window.localStorage.clear();

    EmojiPicker.configs = () => ({
      'dataUrls': {
        'emoji': [
          '/emoji_test_ordering_start.json',
          '/emoji_test_ordering_remaining.json',
        ],
        'emoticon': ['/emoticon_test_ordering.json'],
        'symbol': ['/symbol_test_ordering.json'],
      },
    });

    emojiPicker =
        /** @type {!EmojiPicker} */ (document.createElement('emoji-picker'));

    findInEmojiPicker = (...path) => deepQuerySelector(emojiPicker, path);

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
      'If matching, search should return both emoji and emoticon results.',
      () => {
        emojiSearch.search = 'face';
        flush();
        const emojiResults = findInEmojiPicker('emoji-search', 'emoji-group')
                                 .shadowRoot.querySelectorAll('.emoji-button');
        assertGT(emojiResults.length, 0);
        const emoticonResults =
          findInEmojiPicker(
            'emoji-search', 'emoji-group[category="emoticon"]')
              .shadowRoot.querySelectorAll('.emoji-button');
        assertGT(emoticonResults.length, 0);
      });

  test(
      'Search should display meaningful output when no result is found.',
      () => {
        emojiSearch.search = 'zyxt';
        flush();
        const message = findInEmojiPicker('emoji-search', '.no-result');
        assert(message);
        assertEquals(message.innerText, 'No result found');
      });

  test(
      'If there is only one emoji returned, pressing Enter triggers the ' +
          'clicking event.',
      async () => {
        emojiSearch.search = 'zombi';
        await flush();
        const enterEvent = new KeyboardEvent(
            'keydown', {cancelable: true, key: 'Enter', keyCode: 13});
        const buttonClickPromise = new Promise(
            (resolve) =>
                emojiPicker.addEventListener(EMOJI_BUTTON_CLICK, (event) => {
                  assertEquals('🧟', event.detail.text);
                  assertEquals('zombie', event.detail.name.trim());
                  resolve();
                }));
        emojiSearch.onSearchKeyDown(enterEvent);
        await waitWithTimeout(
            buttonClickPromise, 1000,
            'Failed to receive emoji button click event.');
      });

  test(
      'If there is only emoticon returned, pressing Enter triggers the ' +
          'clicking event.',
      async () => {
        emojiSearch.search = 'cat';
        await flush();
        const enterEvent = new KeyboardEvent(
            'keydown', {cancelable: true, key: 'Enter', keyCode: 13});

        const buttonClickPromise = new Promise(
            (resolve) =>
                emojiPicker.addEventListener(EMOJI_BUTTON_CLICK, (event) => {
                  assertEquals('=^.^=', event.detail.text);
                  resolve();
                }));

        emojiSearch.onSearchKeyDown(enterEvent);
        await waitWithTimeout(
            buttonClickPromise, 1000,
            'Failed to receive emoji button click event.');
      });
});