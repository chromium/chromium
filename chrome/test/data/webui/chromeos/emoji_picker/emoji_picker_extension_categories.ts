// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EMOJI_TEXT_BUTTON_CLICK} from 'chrome://emoji-picker/emoji_picker.js';
import {CrIconButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import {assert} from 'chrome://resources/js/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertGT, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {initialiseEmojiPickerForTest, waitForCondition, waitWithTimeout} from './emoji_picker_test_util.js';

const ACTIVE_CATEGORY_BUTTON = 'category-button-active';

function isCategoryButtonActive(element: Element|null|undefined) {
  assert(element, 'category button element should not be null.');
  return element!.classList.contains(ACTIVE_CATEGORY_BUTTON);
}

function categoryGroupSelector(category: string) {
  return `emoji-group[category="${category}"]:not(.history)`;
}

function historyGroupSelector(category: string) {
  return `[data-group="${category}-history"] > ` +
      `emoji-group[category="${category}"]`;
}

const CATEGORY_LIST = ['emoji', 'symbol', 'emoticon'];

export function categoryTestSuite(category: string) {
  suite(`emoji-picker-extension-${category}`, () => {
    let categoryIndex: number;
    const {emojiPicker, findInEmojiPicker, findEmojiFirstButton, readyPromise} =
        initialiseEmojiPickerForTest();

    setup(async () => {
      await readyPromise;

      categoryIndex = CATEGORY_LIST.indexOf(category);
    });

    test(category + ' category button is initialized correctly', () => {
      const categoryButton =
          findInEmojiPicker('emoji-search')!.shadowRoot!
              .querySelectorAll('emoji-category-button')[categoryIndex]!
              .shadowRoot!.querySelector('cr-icon-button');
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
        async () => {
          const allCategoryButtons =
              Array
                  .from(findInEmojiPicker('emoji-search')!.shadowRoot!
                            .querySelectorAll('emoji-category-button')
                            .values())
                  .map(
                      item => item.shadowRoot!.querySelector('cr-icon-button'));
          const categoryButton = allCategoryButtons[categoryIndex];
          categoryButton!.click();
          flush();
          await waitForCondition(
              () => isCategoryButtonActive(categoryButton),
              'wait for category button active');
          assertTrue(isCategoryButtonActive(categoryButton));
          allCategoryButtons.forEach((categoryButtonItem, index) => {
            if (index !== categoryIndex) {
              assertFalse(isCategoryButtonActive(categoryButtonItem));
            }
          });
        });

    test('category group elements should be rendered.', () => {
      const numGroups =
          emojiPicker.categoriesData
              .filter(item => item.category === category && !item.searchOnly)
              .length;
      const numRenderedGroups =
          emojiPicker.shadowRoot!
              .querySelectorAll(categoryGroupSelector(category))
              .length;
      assertGT(numGroups, 0);
      assertEquals(numGroups, numRenderedGroups);
    });

    test(
        `each ${category} group should have the correct heading and correct` +
            `number of ${category} entries.`,
        () => {
          const allGroups = emojiPicker.shadowRoot!.querySelectorAll(
              categoryGroupSelector(category));

          const data = emojiPicker.categoriesData.filter(
              item => item.category === category && !item.searchOnly);
          const groupElements = emojiPicker.categoriesGroupElements.filter(
              item => item.category === category);

          // The first group element is 'Recently used'.
          assertEquals(groupElements[0]!.name, 'Recently used');

          // Emoticon group elements are created for all data + history.
          assertEquals(groupElements.length, data.length + 1);

          for (let idx = 0; idx < allGroups.length; ++idx) {
            const group = allGroups[idx];
            const actualFirstGroupName =
                (group!.shadowRoot!.querySelector('#heading-left')!.innerHTML
                     .trim())!.replace('&amp;', '&');
            const expectedFirstGroupName = groupElements[idx + 1]!.name;
            assertEquals(expectedFirstGroupName, actualFirstGroupName);
            assertEquals(
                groupElements[idx + 1]!.emoji.length,
                group!.shadowRoot!.querySelectorAll('.emoji-button').length);
          }
        });

    test(
        `clicking at a ${category} button should trigger the clicking event` +
            `correct ${category} string and name.`,
        async () => {
          const firstButton = await waitForCondition(
              () => findEmojiFirstButton(categoryGroupSelector(category)),
              'Wait for first button to render');
          const groupElements = emojiPicker.categoriesGroupElements.filter(
              item => item.category === category && !item.isHistory);
          const expectedString = groupElements[0]!.emoji[0]!.base.string;
          const expectedName = groupElements[0]!.emoji[0]!.base.name;
          const buttonClickPromise = new Promise<void>(
              (resolve) => emojiPicker.addEventListener(
                  EMOJI_TEXT_BUTTON_CLICK, (event) => {
                    assertEquals(expectedString, event.detail.text);
                    assertEquals(expectedName, event.detail.name);
                    resolve();
                  }));
          firstButton!.click();
          await flush();
          await waitWithTimeout(
              buttonClickPromise, 1000,
              `Failed to receive ${category} button click event`);
        });

    test(`recently used ${category} group should be hidden when empty.`, () => {
      assertEquals(
          findInEmojiPicker(historyGroupSelector(category))!.style.display, '');
    });

    test(
        `history tab button must be disabled when the ${category} history` +
            ' is empty.',
        () => {
          // It is assumed that the order of categoryList is the same as
          // buttons.
          const categoryButton =
              findInEmojiPicker('emoji-search')!.shadowRoot!
                  .querySelectorAll('emoji-category-button')[categoryIndex]!
                  .shadowRoot!.querySelector('cr-icon-button');
          categoryButton!.click();
          flush();
          const historyTab = findInEmojiPicker(
              `#tabs emoji-group-button[data-group="${category}-history"]`,
              'cr-icon-button');
          assertTrue((historyTab as CrIconButtonElement).disabled);
        });

    test(
        `clicking at recently used ${category} buttons should trigger ` +
            `${category} insertion.`,
        async () => {
          emojiPicker.updateIncognitoState(false);

          const emojiButton =
              findEmojiFirstButton(categoryGroupSelector(category));
          emojiButton!.click();

          const recentlyUsedButton = await waitForCondition(
              () => findEmojiFirstButton(historyGroupSelector(category)),
              'Wait for history element to render');
          const buttonClickPromise = new Promise<void>(
              (resolve) => emojiPicker.addEventListener(
                  EMOJI_TEXT_BUTTON_CLICK, (event) => {
                    assertEquals(
                        emojiButton!.innerHTML.trim(), event.detail.text);
                    resolve();
                  }));

          recentlyUsedButton!.click();
          await waitWithTimeout(
              buttonClickPromise, 1000,
              `Clicking at recently used ${category} buttons does not ` +
                  `trigger ${category} insertion.`);
        });

    test(
        `recently used ${category} group should contain the ` +
            `correct ${category}  after it is clicked.`,
        async () => {
          emojiPicker.updateIncognitoState(false);

          const emojiButton =
              findEmojiFirstButton(categoryGroupSelector(category));
          emojiButton!.click();

          const recentEmojiButton = await waitForCondition(
              () => findEmojiFirstButton(historyGroupSelector(category)),
              'Wait for recent to render');
          assert(recentEmojiButton);

          const recentlyUsedEmoji = findInEmojiPicker(historyGroupSelector(
              category))!.shadowRoot!.querySelectorAll('.emoji-button');
          assertEquals(1, recentlyUsedEmoji.length);
        });
  });
}
