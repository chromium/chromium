// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EmojiPicker} from 'chrome://emoji-picker/emoji_picker.js';
import {EMOJI_DATA_LOADED} from 'chrome://emoji-picker/events.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from '../../chai_assert.js';
import {deepQuerySelector, waitForCondition} from './emoji_picker_test_util.js';

const ACTIVE_CATEGORY_BUTTON = 'category-button-active';

function isCategoryButtonActive(element) {
  assert(element, 'category button element should not be null.');
  return element.classList.contains(ACTIVE_CATEGORY_BUTTON);
}

suite('emoji-picker-extension', () => {
  /** @type {!EmojiPicker} */
  let emojiPicker;
  /** @type {function(...!string): ?HTMLElement} */
  let findInEmojiPicker;

  setup(() => {
    // Reset DOM state.
    document.body.innerHTML = '';

    emojiPicker =
        /** @type {!EmojiPicker} */ (document.createElement('emoji-picker'));

    findInEmojiPicker = (...path) => deepQuerySelector(emojiPicker, path);

    // Wait until emoji data is loaded before executing tests.
    return new Promise((resolve) => {
      emojiPicker.addEventListener(EMOJI_DATA_LOADED, resolve);
      document.body.appendChild(emojiPicker);
      flush();
    });
  });

  test('emoji category button should be active by default.', () => {
    const emojiCategoryButton = findInEmojiPicker(
        'emoji-search', 'emoji-category-button', 'cr-icon-button');
    assertTrue(isCategoryButtonActive(emojiCategoryButton));
  });

  test('emoticon category button should be inactive by default.', () => {
    const emoticonCategoryButton = findInEmojiPicker(
        'emoji-search', 'emoji-category-button:last-of-type', 'cr-icon-button');
    assertFalse(isCategoryButtonActive(emoticonCategoryButton));
  });

  test(
      'emoticon category button should be active after clicking at it.',
      async () => {
        const emojiCategoryButton = findInEmojiPicker(
            'emoji-search', 'emoji-category-button', 'cr-icon-button');
        const emoticonCategoryButton = findInEmojiPicker(
            'emoji-search', 'emoji-category-button:last-of-type',
            'cr-icon-button');
        emoticonCategoryButton.click();
        await waitForCondition(
            () => isCategoryButtonActive(emoticonCategoryButton) &&
                !isCategoryButtonActive(emojiCategoryButton),
            'Emoticon category button failed to become active or ' +
                'emoji category button failed to become inactive.');
      });
});