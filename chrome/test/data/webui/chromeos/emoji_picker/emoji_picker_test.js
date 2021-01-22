// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EmojiGroupButton} from 'chrome://emoji-picker/emoji_group_button.js';
import {EmojiPicker} from 'chrome://emoji-picker/emoji_picker.js';
import {DATA_LOADED_EVENT} from 'chrome://emoji-picker/events.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {deepQuerySelector} from './emoji_picker_test_util.js';

const ACTIVE_CLASS = 'emoji-group-active';

/**
 * Checks if the given emoji-group-button element is activated.
 * @param {?Element} element element to check.
 * @return {boolean} true if active, false otherwise.
 */
function isGroupButtonActive(element) {
  return element ? element.classList.contains(ACTIVE_CLASS) : false;
}

suite('<emoji-picker>', () => {
  /** @type {!EmojiPicker} */
  let emojiPicker;
  /** @type {function(!Array<!string>): ?HTMLElement} */
  let findInEmojiPicker;

  setup(() => {
    // Reset DOM state.
    document.body.innerHTML = '';
    emojiPicker =
        /** @type {!EmojiPicker} */ (document.createElement('emoji-picker'));
    emojiPicker.emojiDataUrl = '/emoji_test_ordering.json';

    findInEmojiPicker = (path) => deepQuerySelector(emojiPicker, path);

    // Wait until emoji data is loaded before executing tests.
    return new Promise((resolve, reject) => {
      emojiPicker.addEventListener(DATA_LOADED_EVENT, resolve);
      document.body.appendChild(emojiPicker);
      flush();
    });
  });

  test('custom element should be defined', () => {
    assert(customElements.get('emoji-picker') != null);
  });

  test('first tab should be active by default', () => {
    const button = findInEmojiPicker(['emoji-group-button:first-child', 'div']);
    assert(isGroupButtonActive(button));
  });

  test('second tab should be inactive by default', () => {
    const button =
        findInEmojiPicker(['emoji-group-button:nth-child(2)', 'div']);
    assert(!isGroupButtonActive(button));
  });

  test('clicking second tab should activate it and deactivate others', () => {
    const firstButton =
        findInEmojiPicker(['emoji-group-button:nth-child(1)', 'div']);
    const secondButton =
        findInEmojiPicker(['emoji-group-button:nth-child(2)', 'div']);
    secondButton.click();
    assert(isGroupButtonActive(secondButton));
    assert(!isGroupButtonActive(firstButton));
  });
});
