// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EmojiPicker} from 'chrome://emoji-picker/emoji_picker.js';
import {EmojiPickerApiProxyImpl} from 'chrome://emoji-picker/emoji_picker_api_proxy.js';
import {EMOJI_PICKER_READY} from 'chrome://emoji-picker/events.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse} from 'chrome://webui-test/chromeos/chai_assert.js';

import {completePendingMicrotasks, deepQuerySelector, isGroupButtonActive, timeout, waitForCondition} from './emoji_picker_test_util.js';

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
  /** @type {function(...!string): ?HTMLElement} */
  let findEmojiFirstButton;
  /** @type {Array<string>} */
  const categoryList = ['emoji', 'symbol', 'emoticon'];

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

    // Wait until emoji data is loaded before executing tests.
    return new Promise((resolve) => {
      emojiPicker.addEventListener(EMOJI_PICKER_READY, () => {
        flush();
        resolve();
      });
      document.body.appendChild(emojiPicker);
    });
  });

  test(
      `the first tab of the next pagination should be active when clicking at
        either chevron.`,
      async () => {
        const leftChevron = findInEmojiPicker('#left-chevron');
        const rightChevron = findInEmojiPicker('#right-chevron');
        const emoticonCategoryButton = findInEmojiPicker(
            'emoji-search', 'emoji-category-button:last-of-type',
            'cr-icon-button');
        emoticonCategoryButton.click();
        await flush();
        const firstEmoticonTabInFirstPage =
            findInEmojiPicker('.pagination text-group-button', 'cr-button');
        const firstEmoticonTabInSecondPage = findInEmojiPicker(
            '.pagination + .pagination', 'text-group-button', 'cr-button');
        rightChevron.click();

        await flush();
        await waitForCondition(
            () => !isGroupButtonActive(firstEmoticonTabInFirstPage) &&
                isGroupButtonActive(firstEmoticonTabInSecondPage));
        leftChevron.click();
        await flush();
        await waitForCondition(
            () => !isGroupButtonActive(firstEmoticonTabInSecondPage) &&
                isGroupButtonActive(firstEmoticonTabInFirstPage));
      });

  test(
      'Scrolling to an emoticon group should activate the emoticon category ' +
          'button.',
      async () => {
        const emojiCategoryButton = findInEmojiPicker(
            'emoji-search', 'emoji-category-button', 'cr-icon-button');
        const emoticonCategoryButton = findInEmojiPicker(
            'emoji-search', 'emoji-category-button:last-of-type',
            'cr-icon-button');

        const emoticonTestGroupId = '20';
        emojiPicker.scrollToGroup(emoticonTestGroupId);

        await waitForCondition(
            () => isCategoryButtonActive(emoticonCategoryButton) &&
                !isCategoryButtonActive(emojiCategoryButton));
      });

  test(
      'Scrolling to an emoticon group should activate the corresponding ' +
          'subcategory tab.',
      async () => {
        const emoticonTestGroupId = '20';
        emojiPicker.scrollToGroup(emoticonTestGroupId);
        const emoticonTabButton = await waitForCondition(
            () => findInEmojiPicker(
                `.tab[data-group='${emoticonTestGroupId}']`, 'cr-button'));

        await waitForCondition(
            () => isGroupButtonActive(emoticonTabButton),
            'The tab where the group is scrolled to failed to become active');
      });

  test('Scrolling to an emoticon group should update chevrons.', async () => {
    const leftChevron = findInEmojiPicker('#left-chevron');
    const rightChevron = findInEmojiPicker('#right-chevron');
    const emoticonTestGroupId = '25';

    emojiPicker.scrollToGroup(emoticonTestGroupId);
    // when scrolling to the next page, the chevron display needs to be updated.
    await waitForCondition(
        () => leftChevron.style.display === 'flex' &&
            rightChevron.style.display === 'flex');
  });

  test(
      'Clicking on a category during search clears search and scrolls to the' +
          'correct location',
      async () => {
        const emojiCategoryButton = findInEmojiPicker(
            'emoji-search', 'emoji-category-button', 'cr-icon-button');
        const emoticonCategoryButton = findInEmojiPicker(
            'emoji-search', 'emoji-category-button:last-of-type',
            'cr-icon-button');
        const emojiGroups = findInEmojiPicker('#groups');
        emoticonCategoryButton.click();
        const targetScroll = emojiGroups.scrollTop;
        emojiCategoryButton.click();

        findInEmojiPicker('emoji-search').setSearchQuery('face');
        await waitForCondition(
            () => findInEmojiPicker('emoji-search', 'emoji-group'));
        emoticonCategoryButton.click();

        await waitForCondition(() => emojiGroups.scrollTop === targetScroll);
        assertFalse(findInEmojiPicker('emoji-search').searchNotEmpty());
      });

  test(
      'Focusing on emoji tab groups does should not scroll the tabs section',
      async () => {
        const emojiGroupButton = findInEmojiPicker(
            '#tabs', 'emoji-group-button:last-of-type', 'cr-icon-button');
        emojiGroupButton.focus();
        await waitForCondition(
            () => findInEmojiPicker(
                '#tabs', 'emoji-group-button:last-of-type:focus-within'));
        // wait for any potential smooth scrolling to take place
        await timeout(400);

        assertEquals(findInEmojiPicker('#tabs').scrollLeft, 0);
      });

  test(
      'Focusing on emoticon tab groups does should not scroll the tabs section',
      async () => {
        const categoryButton = findInEmojiPicker(
            'emoji-search', 'emoji-category-button:last-of-type',
            'cr-icon-button');
        categoryButton.click();
        await waitForCondition(() => isCategoryButtonActive(categoryButton));
        await completePendingMicrotasks();
        const emojiGroupButton = findInEmojiPicker(
            '#tabs', 'text-group-button:last-of-type', 'cr-button');
        emojiGroupButton.focus();
        await waitForCondition(
            () => findInEmojiPicker(
                '#tabs', 'text-group-button:last-of-type:focus-within'));
        // wait for any potential smooth scrolling to take place
        await timeout(400);

        assertEquals(findInEmojiPicker('#tabs').scrollLeft, 0);
      });
});
