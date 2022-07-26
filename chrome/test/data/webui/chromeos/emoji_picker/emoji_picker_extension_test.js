// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EmojiPicker} from 'chrome://emoji-picker/emoji_picker.js';
import {EmojiPickerApiProxyImpl} from 'chrome://emoji-picker/emoji_picker_api_proxy.js';
import {EMOJI_BUTTON_CLICK, EMOJI_PICKER_READY} from 'chrome://emoji-picker/events.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertGT, assertTrue} from '../../chai_assert.js';
import {deepQuerySelector, isGroupButtonActive, timeout, waitForCondition, waitWithTimeout} from './emoji_picker_test_util.js';

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
  /** @type {string} */
  const emoticonGroupSelector =
        'emoji-group[category="emoticon"]:not(.history)';
  /** @type {string} */
  const emoticonHistoryGroupSelector =
        '[data-group="emoticon-history"] > emoji-group[category="emoticon"]';

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
      'emoticon category button should be active after clicking at it.', () => {
        const emojiCategoryButton = findInEmojiPicker(
            'emoji-search', 'emoji-category-button', 'cr-icon-button');
        const emoticonCategoryButton = findInEmojiPicker(
            'emoji-search', 'emoji-category-button:last-of-type',
            'cr-icon-button');
        emoticonCategoryButton.click();
        flush();
        assertTrue(isCategoryButtonActive(emoticonCategoryButton));
        assertFalse(isCategoryButtonActive(emojiCategoryButton));
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

  test('all emoticon groups should be rendered.', () => {
    const numEmoticons = emojiPicker.categoriesData.filter(
        item => item.category === 'emoticon').length;
    assertGT(numEmoticons, 0);
    assertEquals(
        numEmoticons,
        emojiPicker.shadowRoot.querySelectorAll(
            emoticonGroupSelector).length);
  });

  test(
      'each emoticon group should have the correct heading and correct' +
          'number of emoticon entries.',
      async () => {
        const allEmoticonGroups =
            emojiPicker.shadowRoot.querySelectorAll(emoticonGroupSelector);

        const emoticonData = emojiPicker.categoriesData.filter(
            item => item.category === 'emoticon');
        const emoticonGroupElements =
            emojiPicker.categoriesGroupElements.filter(
                item => item.category === 'emoticon');

        // The first group element is 'Recently used'.
        assertEquals(emoticonGroupElements[0].name, 'Recently used');

        // Emoticon group elements are created for all data + history.
        assertEquals(emoticonGroupElements.length, emoticonData.length + 1);

        for (let idx = 0; idx < allEmoticonGroups.length; ++idx) {
          const group = allEmoticonGroups[idx];
          const actualFirstGroupName =
              group.shadowRoot.querySelector('#heading-left').innerHTML.trim();
          const expectedFirstGroupName = emoticonGroupElements[idx+1].name;
          assertEquals(expectedFirstGroupName, actualFirstGroupName);

          const expectedNumberOfEmoticons =
            emoticonGroupElements[idx+1].emoji.length;
          await waitForCondition(
              () => expectedNumberOfEmoticons ===
                  group.shadowRoot.querySelectorAll('.emoji-button').length);
        }
      });

  test(
      'clicking at an emoticon button should trigger the clicking event with ' +
          'correct emoticon string and name.',
      async () => {
        const firstEmoticonButton = await waitForCondition(
            () => findEmojiFirstButton(emoticonGroupSelector));
        const emoticonGroupElements =
            emojiPicker.categoriesGroupElements.filter(
                item => item.category === 'emoticon' && !item.isHistory);
        const expectedEmoticonString =
            emoticonGroupElements[0].emoji[0].base.string;
        const expectedEmoticonName =
            emoticonGroupElements[0].emoji[0].base.name;
        const buttonClickPromise = new Promise(
            (resolve) =>
                emojiPicker.addEventListener(EMOJI_BUTTON_CLICK, (event) => {
                  assertEquals(expectedEmoticonString, event.detail.text);
                  assertEquals(expectedEmoticonName, event.detail.name);
                  resolve();
                }));
        firstEmoticonButton.click();
        await flush();
        await waitWithTimeout(
            buttonClickPromise, 1000,
            'Failed to receive emoticon button click event');
      });

  test('there should be no recently used emoticon group when empty.', () => {
    assert(!findInEmojiPicker('[data-group=emoticon-history] emoticon-group'));
  });

  test(
      'history tab button must be disabled when the emoticon history is empty.',
      () => {
        const emoticonCategoryButton = findInEmojiPicker(
            'emoji-search', 'emoji-category-button:last-of-type',
            'cr-icon-button');
        emoticonCategoryButton.click();
        flush();
        const emoticonHistoryTab = findInEmojiPicker(
            '.pagination [data-group=emoticon-history]', 'cr-icon-button');
        assertTrue(emoticonHistoryTab.disabled);
      });

  test(
      'clicking at recently used emoticon buttons should trigger emoticon ' +
          'insertion.',
      async () => {
        emojiPicker.updateIncognitoState(false);

        const emoticonButton = findEmojiFirstButton(emoticonGroupSelector);
        emoticonButton.click();

        const recentlyUsedEmoticonButton = await waitForCondition(
            () => findEmojiFirstButton(emoticonHistoryGroupSelector));
        const buttonClickPromise = new Promise(
            (resolve) =>
                emojiPicker.addEventListener(EMOJI_BUTTON_CLICK, (event) => {
                  assertEquals(
                      emoticonButton.innerHTML.trim(), event.detail.text);
                  resolve();
                }));

        recentlyUsedEmoticonButton.click();
        await waitWithTimeout(
            buttonClickPromise, 1000,
            'Clicking at recently used emoticon buttons does not trigger ' +
                'emoticon insertion.');
      });

  test(
      'recently used emoticon group should contain the correct emoticon ' +
          'after it is clicked.',
      async () => {
        emojiPicker.updateIncognitoState(false);
        const emoticonButton = findEmojiFirstButton(emoticonGroupSelector);
        emoticonButton.click();

        const recentEmoticonButton = await waitForCondition(
            () => findEmojiFirstButton(emoticonHistoryGroupSelector));
        assert(recentEmoticonButton);

        const recentlyUsedEmoticons =
            findInEmojiPicker(
                emoticonHistoryGroupSelector,
                )
                .shadowRoot.querySelectorAll('.emoji-button');
        assertEquals(1, recentlyUsedEmoticons.length);
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
});