// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EmojiGroupButton} from 'chrome://emoji-picker/emoji_group_button.js';
import {EmojiPicker} from 'chrome://emoji-picker/emoji_picker.js';
import {DATA_LOADED_EVENT} from 'chrome://emoji-picker/events.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertGT} from '../../chai_assert.js';

import {deepQuerySelector, waitForCondition} from './emoji_picker_test_util.js';

const ACTIVE_CLASS = 'emoji-group-active';

/**
 * Checks if the given emoji-group-button element is activated.
 * @param {?Element} element element to check.
 * @return {boolean} true if active, false otherwise.
 */
function isGroupButtonActive(element) {
  assert(element, 'group button element should not be null');
  return element.classList.contains(ACTIVE_CLASS);
}


suite('<emoji-picker>', () => {
  /** @type {!EmojiPicker} */
  let emojiPicker;
  /** @type {function(!Array<!string>): ?HTMLElement} */
  let findInEmojiPicker;

  setup(() => {
    // Reset DOM state.
    document.body.innerHTML = '';
    window.localStorage.clear();

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

  test('clicking second tab should activate it and scroll', async () => {
    // store initial scroll position of emoji groups.
    const emojiGroups = findInEmojiPicker(['.emoji-groups']);
    const initialScroll = emojiGroups.scrollTop;

    const firstButton =
        findInEmojiPicker(['emoji-group-button[data-group="history"]', 'div']);
    // note: use second non-history group because history will be missing
    // since it is empty.
    const secondButton =
        findInEmojiPicker(['emoji-group-button[data-group="1"]', 'div']);

    // wait so emoji-groups render and we have something to scroll to.
    await waitForCondition(
        () => findInEmojiPicker(['[data-group="1"] > emoji-group', 'button']));
    secondButton.click();

    // wait while waiting for scroll to happen and update buttons.
    await waitForCondition(
        () => isGroupButtonActive(secondButton) &&
            !isGroupButtonActive(firstButton));

    const newScroll = emojiGroups.scrollTop;
    assertGT(newScroll, initialScroll);
  });

  test('recently used should be hidden when empty', () => {
    const recentlyUsed =
        findInEmojiPicker(['[data-group=history] > emoji-group']);
    assert(!recentlyUsed);
  });

  test('recently used should be populated after emoji is clicked', async () => {
    // yield to allow emoji-group and emoji buttons to render.
    const emojiButton = await waitForCondition(
        () => findInEmojiPicker([
          '[data-group="0"] > emoji-group', '.emoji-group-emoji', 'button'
        ]));
    emojiButton.click();

    // wait until emoji exists in recently used section.
    const recentlyUsed = await waitForCondition(
        () => findInEmojiPicker(
            ['[data-group=history] > emoji-group', 'button']));

    // check text is correct.
    const recentText = recentlyUsed.innerText;
    assert(recentText.includes(String.fromCodePoint(128512)));
  });
});
