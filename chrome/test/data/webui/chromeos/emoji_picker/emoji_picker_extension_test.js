// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EmojiPicker} from 'chrome://emoji-picker/emoji_picker.js';
import {EmojiPickerApiProxyImpl} from 'chrome://emoji-picker/emoji_picker_api_proxy.js';
import {EMOJI_BUTTON_CLICK, V2_CONTENT_LOADED} from 'chrome://emoji-picker/events.js';
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
  const emoticonGroupSelector = 'emoji-group[category="emoticon"]';
  /** @type {string} */
  const emoticonHistoryGroupSelector =
        '[data-group="emoticon-history"] > emoji-group[category="emoticon"]';

  setup(() => {
    // Reset DOM state.
    document.body.innerHTML = '';
    window.localStorage.clear();

    emojiPicker =
        /** @type {!EmojiPicker} */ (document.createElement('emoji-picker'));
    emojiPicker.emojiDataUrl = '/emoji_test_ordering';
    emojiPicker.emoticonDataUrl = '/emoticon_test_ordering.json';

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
      emojiPicker.addEventListener(V2_CONTENT_LOADED, () => {
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
    assertEquals(
        emojiPicker.emoticonData.length,
        emojiPicker.shadowRoot.querySelectorAll(
            emoticonGroupSelector).length);
  });

  test(
      'each emoticon group should have the correct heading and correct' +
          'number of emoticon entries.',
      async () => {
        const allEmoticonGroups =
            emojiPicker.shadowRoot.querySelectorAll(emoticonGroupSelector);
        for (let idx = 0; idx < allEmoticonGroups.length; ++idx) {
          const group = allEmoticonGroups[idx];
          const actualFirstGroupName =
              group.shadowRoot.querySelector('#heading-left').innerHTML.trim();
          const expectedFirstGroupName = emojiPicker.emoticonData[idx].group;
          assertEquals(expectedFirstGroupName, actualFirstGroupName);

          const expectedNumberOfEmoticons =
              emojiPicker.emoticonData[idx].emoji.length;
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
        const expectedEmoticonString =
            emojiPicker.emoticonData[0].emoji[0].base.string;
        const expectedEmoticonName =
            emojiPicker.emoticonData[0].emoji[0].base.name;
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
        EmojiPickerApiProxyImpl.getInstance().isIncognitoTextField = () =>
            new Promise((resolve) => resolve({incognito: false}));

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
        EmojiPickerApiProxyImpl.getInstance().isIncognitoTextField = () =>
            new Promise((resolve) => resolve({incognito: false}));

        const emoticonButton = findEmojiFirstButton(emoticonGroupSelector);
        emoticonButton.click();

        const recentEmoticonButton = await waitForCondition(
            () => findEmojiFirstButton(emoticonHistoryGroupSelector));
        assert(recentEmoticonButton);

        const recentlyUsedEmoticons =
            findInEmojiPicker(emoticonHistoryGroupSelector
                ).shadowRoot.querySelectorAll('.emoji-button');
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

        const emoticonTestGroupId = '10';
        emojiPicker.scrollToGroup(emoticonTestGroupId);

        await waitForCondition(
            () => isCategoryButtonActive(emoticonCategoryButton) &&
                !isCategoryButtonActive(emojiCategoryButton));
      });
  test(
      'Scrolling to an emoticon group should activate the corresponding ' +
          'subcategory tab.',
      async () => {
        const emoticonTestGroupId = '10';
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
    const emoticonTestGroupId = '15';

    emojiPicker.scrollToGroup(emoticonTestGroupId);
    // when scrolling to the next page, the chevron display needs to be updated.
    await waitForCondition(
        () => leftChevron.style.display === 'flex' &&
            rightChevron.style.display === 'flex');
  });
});