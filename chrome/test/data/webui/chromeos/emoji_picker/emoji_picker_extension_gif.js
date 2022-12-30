// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EmojiPicker} from 'chrome://emoji-picker/emoji_picker.js';
import {EmojiPickerApiProxyImpl} from 'chrome://emoji-picker/emoji_picker_api_proxy.js';
import {EMOJI_PICKER_READY} from 'chrome://emoji-picker/events.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {deepQuerySelector} from './emoji_picker_test_util.js';

const ACTIVE_CATEGORY_BUTTON = 'category-button-active';

function isCategoryButtonActive(element) {
  assert(element, 'category button element should not be null.');
  return element.classList.contains(ACTIVE_CATEGORY_BUTTON);
}

export function GifTestSuite(category) {
  suite(`emoji-picker-extension-${category}`, () => {
    /** @type {!EmojiPicker} */
    let emojiPicker;
    /** @type {function(...!string): ?HTMLElement} */
    let findInEmojiPicker;
    /** @type {function(...!string): ?HTMLElement} */
    let findEmojiFirstButton;
    /** @type {Array<string>} */
    const categoryList = ['emoji', 'symbol', 'emoticon', 'gif'];
    /** @type {number} */
    let categoryIndex;

    setup(() => {
      // Reset DOM state.
      document.body.innerHTML = '';
      window.localStorage.clear();

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
          'gif': ['/gif_test_ordering.json'],
        },
      });

      emojiPicker =
          /** @type {!EmojiPicker} */ (document.createElement('emoji-picker'));

      findInEmojiPicker = (...path) => deepQuerySelector(emojiPicker, path);

      findEmojiFirstButton = (...path) => {
        const emojiElement = deepQuerySelector(emojiPicker, path);
        if (emojiElement) {
          return emojiElement.firstEmojiButton();
        }
        return null;
      };

      categoryIndex = categoryList.indexOf(category);

      // Wait until emoji data is loaded before executing tests.
      return new Promise((resolve) => {
        emojiPicker.addEventListener(EMOJI_PICKER_READY, () => {
          flush();
          resolve();
        });
        document.body.appendChild(emojiPicker);
      });
    });

    test(category + ' category button is initialized correctly', () => {
      const categoryButton =
          findInEmojiPicker('emoji-search')
              .shadowRoot
              .querySelectorAll('emoji-category-button')[categoryIndex]
              .shadowRoot.querySelector('cr-icon-button');
      if (categoryIndex === 0) {
        assertTrue(
            isCategoryButtonActive(categoryButton),
            'First button must be active.');
      } else {
        assertFalse(
            isCategoryButtonActive(categoryButton),
            'All buttons must be inactive except the first one.');
      }
    });

    test(
        category + ' category button should be active after clicking at it.',
        () => {
          const allCategoryButtons =
              Array
                  .from(
                      findInEmojiPicker('emoji-search')
                          .shadowRoot.querySelectorAll('emoji-category-button')
                          .values())
                  .map(item => item.shadowRoot.querySelector('cr-icon-button'));
          const categoryButton = allCategoryButtons[categoryIndex];
          categoryButton.click();
          flush();
          assertTrue(isCategoryButtonActive(categoryButton));
          allCategoryButtons.forEach((categoryButtonItem, index) => {
            if (index !== categoryIndex) {
              assertFalse(isCategoryButtonActive(categoryButtonItem));
            }
          });
        });
  });
}
