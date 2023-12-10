// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EmojiPickerApp, EmojiSearch} from 'chrome://emoji-picker/emoji_picker.js';
import {assert} from 'chrome://resources/js/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';

import {completePendingMicrotasks, initialiseEmojiPickerForTest, isGroupButtonActive, timeout, waitForCondition} from './emoji_picker_test_util.js';

const ACTIVE_CATEGORY_BUTTON = 'category-button-active';

function isCategoryButtonActive(element: HTMLElement|null|undefined) {
  assert(element, 'category button element should not be null.');
  return element!.classList.contains(ACTIVE_CATEGORY_BUTTON);
}

suite('emoji-picker-extension', () => {
  let emojiPicker: EmojiPickerApp;
  let findInEmojiPicker: (...path: string[]) => HTMLElement | null;
  let waitUntilFindInEmojiPicker: (...path: string[]) => Promise<HTMLElement>;

  setup(async () => {
    const newPicker = initialiseEmojiPickerForTest();
    emojiPicker = newPicker.emojiPicker;
    findInEmojiPicker = newPicker.findInEmojiPicker;
    waitUntilFindInEmojiPicker = newPicker.waitUntilFindInEmojiPicker;
    await newPicker.readyPromise;
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
        emoticonCategoryButton!.click();
        await flush();
        const firstEmoticonTabInFirstPage = await waitUntilFindInEmojiPicker(
            '.pagination text-group-button', 'cr-button');
        const firstEmoticonTabInSecondPage = await waitUntilFindInEmojiPicker(
            '.pagination + .pagination', 'text-group-button', 'cr-button');
        rightChevron!.click();

        await flush();
        await waitForCondition(
            () => !isGroupButtonActive(firstEmoticonTabInFirstPage) &&
                isGroupButtonActive(firstEmoticonTabInSecondPage),
            'Wait for correct group to be active');
        leftChevron!.click();
        await flush();
        await waitForCondition(
            () => !isGroupButtonActive(firstEmoticonTabInSecondPage) &&
                isGroupButtonActive(firstEmoticonTabInFirstPage),
            'Wait for second group to be active');
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
                !isCategoryButtonActive(emojiCategoryButton),
            'Wait for correct group to be active');
      });

  test(
      'Scrolling to an emoticon group should activate the corresponding ' +
          'subcategory tab.',
      async () => {
        const emoticonTestGroupId = '20';
        emojiPicker.scrollToGroup(emoticonTestGroupId);
        const emoticonTabButton = await waitForCondition(
            () => findInEmojiPicker(
                `.tab[data-group='${emoticonTestGroupId}']`, 'cr-button'),
            'Wait for emoticon button to become active.');

        await waitForCondition(
            () => isGroupButtonActive(emoticonTabButton),
            'The tab where the group is scrolled to failed to become active');
      });

  test('Scrolling to an emoticon group should update chevrons.', async () => {
    const leftChevron = findInEmojiPicker('#left-chevron');
    const rightChevron = findInEmojiPicker('#right-chevron');
    const emoticonTestGroupId = '25';

    emojiPicker.scrollToGroup(emoticonTestGroupId);
    await waitForCondition(
        () => leftChevron!.style.display === 'flex' &&
            rightChevron!.style.display === 'flex',
        'When scrolling to the next page, the chevron display needs to be updated.');
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
        const emojiGroups = await waitUntilFindInEmojiPicker('#groups');
        emoticonCategoryButton!.click();
        const targetScroll = emojiGroups!.scrollTop;
        emojiCategoryButton!.click();

        const emojiSearch = await waitUntilFindInEmojiPicker('emoji-search');
        (emojiSearch as unknown as EmojiSearch)!.setSearchQuery('face');

        await waitForCondition(
            () => findInEmojiPicker('emoji-search', 'emoji-group') !== null,
            'Wait for emoji groups to render');

        emoticonCategoryButton!.click();

        await waitForCondition(
            () => emojiGroups!.scrollTop === targetScroll,
            'Wait for scrolling to complete');

        await waitForCondition(
            () => (findInEmojiPicker('emoji-search') as EmojiSearch)
                      .searchNotEmpty() === false,
            'wait for search results empty');

        assertFalse((findInEmojiPicker('emoji-search') as EmojiSearch)
                        .searchNotEmpty());
      });

  test(
      'Focusing on emoji tab groups does should not scroll the tabs section',
      async () => {
        const emojiGroupButton = findInEmojiPicker(
            '#tabs', 'emoji-group-button:last-of-type', 'cr-icon-button');
        emojiGroupButton!.focus();
        await waitForCondition(
            () => findInEmojiPicker(
                '#tabs', 'emoji-group-button:last-of-type:focus-within'),
            'Wait for correct tab to be focused');
        // wait for any potential smooth scrolling to take place
        await timeout(400);

        assertEquals(findInEmojiPicker('#tabs')!.scrollLeft, 0);
      });

  test(
      'Focusing on emoticon tab groups does should not scroll the tabs section',
      async () => {
        const categoryButton = findInEmojiPicker(
            'emoji-search', 'emoji-category-button:last-of-type',
            'cr-icon-button');
        categoryButton!.click();
        await waitForCondition(
            () => isCategoryButtonActive(categoryButton),
            'Wait for correct tab to be active');
        await completePendingMicrotasks();
        const emojiGroupButton = findInEmojiPicker(
            '#tabs', 'text-group-button:last-of-type', 'cr-button');
        emojiGroupButton!.focus();
        await waitForCondition(
            () => findInEmojiPicker(
                '#tabs', 'text-group-button:last-of-type:focus-within'),
            'Wait for correct group to be focused');
        // wait for any potential smooth scrolling to take place
        await timeout(400);

        assertEquals(findInEmojiPicker('#tabs')!.scrollLeft, 0);
      });
});
