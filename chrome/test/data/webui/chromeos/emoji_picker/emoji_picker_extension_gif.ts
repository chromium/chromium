// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EMOJI_IMG_BUTTON_CLICK, EmojiPickerApiProxy, EmojiPickerApp, TRENDING_GROUP_ID} from 'chrome://emoji-picker/emoji_picker.js';
import {CrIconButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import {assert} from 'chrome://resources/js/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {assertEmojiImageAlt, initialiseEmojiPickerForTest, isGroupButtonActive, timeout, waitForCondition, waitWithTimeout} from './emoji_picker_test_util.js';
import {TestEmojiPickerApiProxy} from './test_emoji_picker_api_proxy.js';

const ACTIVE_CATEGORY_BUTTON = 'category-button-active';
const CATEGORY_LIST = ['emoji', 'symbol', 'emoticon', 'gif'];

function isCategoryButtonActive(element: HTMLElement|null|undefined) {
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

function subcategoryGroupSelector(category: string, subcategory: string) {
  return `[data-group="${subcategory}"] > ` +
      `emoji-group[category="${category}"]`;
}

export function gifTestSuite(category: string) {
  suite(`emoji-picker-extension-${category}`, () => {
    EmojiPickerApiProxy.setInstance(new TestEmojiPickerApiProxy());
    let emojiPicker: EmojiPickerApp;
    let findInEmojiPicker: (...path: string[]) => HTMLElement | null;
    let findEmojiFirstButton: (...path: string[]) =>
        HTMLElement | null | undefined;
    let waitUntilFindInEmojiPicker: (...path: string[]) => Promise<HTMLElement>;
    let scrollDown: (height: number) => void;
    let scrollToBottom: () => void;
    let categoryIndex: number;

    setup(async () => {
      const newPicker = initialiseEmojiPickerForTest();
      emojiPicker = newPicker.emojiPicker;
      findInEmojiPicker = newPicker.findInEmojiPicker;
      findEmojiFirstButton = newPicker.findEmojiFirstButton;
      waitUntilFindInEmojiPicker = newPicker.waitUntilFindInEmojiPicker;
      scrollDown = newPicker.scrollDown;
      scrollToBottom = newPicker.scrollToBottom;
      await newPicker.readyPromise;

      categoryIndex = CATEGORY_LIST.indexOf(category);
    });

    test(category + ' category button is initialized correctly', () => {
      const categoryButton =
          findInEmojiPicker('emoji-search')!.shadowRoot!
              .querySelectorAll('emoji-category-button')[categoryIndex]!
              .shadowRoot!.querySelector('cr-icon-button');
      if (categoryIndex === 0) {
        assertTrue(
            isCategoryButtonActive(categoryButton!),
            'First button must be active.');
      } else {
        assertFalse(
            isCategoryButtonActive(categoryButton!),
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
          await waitForCondition(
              () => isCategoryButtonActive(categoryButton),
              'gif section failed to be active', 5000);
          allCategoryButtons.forEach((categoryButtonItem, index) => {
            if (index !== categoryIndex) {
              assertFalse(isCategoryButtonActive(categoryButtonItem));
            }
          });
        });

    test(
        `history tab button must be disabled when the ${category} history` +
            ' is empty.',
        async () => {
          // It is assumed that the order of categoryList is the same as
          // buttons.
          const categoryButton =
              findInEmojiPicker('emoji-search')!.shadowRoot!
                  .querySelectorAll('emoji-category-button')[categoryIndex]!
                  .shadowRoot!.querySelector('cr-icon-button');
          categoryButton!.click();
          flush();
          const historyTab =
              await waitUntilFindInEmojiPicker(
                  `#tabs emoji-group-button[data-group="${category}-history"]`,
                  'cr-icon-button') as CrIconButtonElement |
              null;
          assertTrue(historyTab!.disabled);
        });

    test(
        `clicking at a ${category} button should trigger the clicking event` +
            `correct ${category} string and name.`,
        async () => {
          const firstButton = await waitForCondition(
              () => findEmojiFirstButton(categoryGroupSelector(category)),
              'wait for emoji button');
          const groupElements = emojiPicker.categoriesGroupElements.filter(
              item => item.category === category && !item.isHistory);
          const expectedString =
              groupElements[0]!.emoji[0]!.base.visualContent!.url.preview;
          const expectedName = groupElements[0]!.emoji[0]!.base.name;
          const buttonClickPromise = new Promise<void>(
              (resolve) => emojiPicker.addEventListener(
                  EMOJI_IMG_BUTTON_CLICK, (event) => {
                    assertEquals(
                        expectedString, event.detail.visualContent.url.preview);
                    assertEquals(expectedName, event.detail.name);
                    resolve();
                  }));
          firstButton!.click();
          await flush();
          await waitWithTimeout(
              buttonClickPromise, 1000,
              `Failed to receive ${category} button click event`);
        });

    test(
        `recently used ${category} group should contain the ` +
            `correct ${category} after it is clicked.`,
        async () => {
          emojiPicker.updateIncognitoState(false);

          const emojiButton =
              findEmojiFirstButton(categoryGroupSelector(category));
          emojiButton!.click();

          const recentEmojiButton = await waitForCondition(
              () => findEmojiFirstButton(historyGroupSelector(category)),
              'wait for recent emoji button to render');
          assert(recentEmojiButton);

          const recentlyUsedEmoji = findInEmojiPicker(historyGroupSelector(
              category))!.shadowRoot!.querySelectorAll('emoji-image');
          assertEquals(1, recentlyUsedEmoji.length);
        });

    test(
        `recently used ${category} group should not contain ` +
            `duplicate ${category}s.`,
        async () => {
          emojiPicker.updateIncognitoState(false);

          const emojiButton =
              findEmojiFirstButton(categoryGroupSelector(category));
          emojiButton!.click();

          const recentEmojiButton = await waitForCondition(
              () => findEmojiFirstButton(historyGroupSelector(category)),
              'wait for recent emoji button to render');
          assert(recentEmojiButton);

          const recentlyUsedEmoji1 = findInEmojiPicker(historyGroupSelector(
              category))!.shadowRoot!.querySelectorAll('emoji-image');
          assertEquals(1, recentlyUsedEmoji1.length);

          // Click the same emoji again
          emojiButton!.click();

          const recentlyUsedEmoji2 = findInEmojiPicker(historyGroupSelector(
              category))!.shadowRoot!.querySelectorAll('emoji-image');
          assertEquals(1, recentlyUsedEmoji2.length);
        });

    test(
        `the first tab of the next pagination should be active when clicking
        at either chevron.`,
        async () => {
          const leftChevron = findInEmojiPicker('#left-chevron');
          const rightChevron = findInEmojiPicker('#right-chevron');
          const gifCategoryButton = findInEmojiPicker(
              'emoji-search', 'emoji-category-button:last-of-type',
              'cr-icon-button');
          gifCategoryButton!.click();
          await flush();
          const firstGifTabInFirstPage = await waitUntilFindInEmojiPicker(
              '.pagination text-group-button', 'cr-button');
          const firstGifTabInSecondPage = await waitUntilFindInEmojiPicker(
              '.pagination + .pagination', 'text-group-button', 'cr-button');
          rightChevron!.click();

          await flush();
          await waitForCondition(
              () => !isGroupButtonActive(firstGifTabInFirstPage) &&
                  isGroupButtonActive(firstGifTabInSecondPage),
              'wait for active tab to switch');
          leftChevron!.click();
          await flush();
          await waitForCondition(
              () => !isGroupButtonActive(firstGifTabInSecondPage) &&
                  isGroupButtonActive(firstGifTabInFirstPage),
              'wait for active tab to switch back');
        });

    test('Trending should display GIFs.', async () => {
      const categoryButton =
          findInEmojiPicker('emoji-search')!.shadowRoot!
              .querySelectorAll('emoji-category-button')[categoryIndex]!
              .shadowRoot!.querySelector('cr-icon-button');
      categoryButton!.click();
      flush();

      await waitForCondition(
          () => emojiPicker.activeInfiniteGroupId === TRENDING_GROUP_ID,
          'wait for trending to be active');

      const gifResults = findInEmojiPicker(subcategoryGroupSelector(
          category,
          emojiPicker.activeInfiniteGroupId!,
          ))!.shadowRoot!.querySelectorAll('emoji-image');
      assertEquals(gifResults.length, 6);
    });

    test('Trending should display GIFs in the correct order.', async () => {
      const categoryButton =
          findInEmojiPicker('emoji-search')!.shadowRoot!
              .querySelectorAll('emoji-category-button')[categoryIndex]!
              .shadowRoot!.querySelector('cr-icon-button');
      categoryButton!.click();
      flush();

      await waitForCondition(
          () => emojiPicker.activeInfiniteGroupId === TRENDING_GROUP_ID,
          'wait for trending to be active');

      const gifResults = findInEmojiPicker(subcategoryGroupSelector(
          category,
          emojiPicker.activeInfiniteGroupId!,
          ))!.shadowRoot!.querySelectorAll('emoji-image');
      assertEquals(gifResults.length, 6);

      // Check display is correct.
      const leftColResults = findInEmojiPicker(subcategoryGroupSelector(
          category, emojiPicker.activeInfiniteGroupId!))!.shadowRoot!
                                 .querySelectorAll<HTMLImageElement>(
                                     'div.left-column > emoji-image');
      assertEquals(leftColResults.length, 3);
      assertEmojiImageAlt(leftColResults[0], 'Trending Left 1');
      assertEmojiImageAlt(leftColResults[1], 'Trending Left 2');
      assertEmojiImageAlt(leftColResults[2], 'Trending Left 3');

      const rightColResults = findInEmojiPicker(subcategoryGroupSelector(
          category, emojiPicker.activeInfiniteGroupId!))!.shadowRoot!
                                  .querySelectorAll<HTMLImageElement>(
                                      'div.right-column > emoji-image');
      assertEquals(rightColResults.length, 3);
      assertEmojiImageAlt(rightColResults[0], 'Trending Right 1');
      assertEmojiImageAlt(rightColResults[1], 'Trending Right 2');
      assertEmojiImageAlt(rightColResults[2], 'Trending Right 3');
    });

    test(
        'User does not load more GIFs if they have not scrolled down' +
            ' far enough',
        async () => {
          const categoryButton =
              findInEmojiPicker('emoji-search')!.shadowRoot!
                  .querySelectorAll('emoji-category-button')[categoryIndex]!
                  .shadowRoot!.querySelector('cr-icon-button');
          categoryButton!.click();
          flush();

          await waitForCondition(
              () => emojiPicker.activeInfiniteGroupId === TRENDING_GROUP_ID,
              'Wait for trending to be active.');

          const gifResults1 = findInEmojiPicker(subcategoryGroupSelector(
              category,
              emojiPicker.activeInfiniteGroupId!,
              ))!.shadowRoot!.querySelectorAll('emoji-image');
          assertEquals(gifResults1.length, 6);

          // Scroll down a little bit to activate checking if we need more GIFs.
          scrollDown(100);

          // Wait for emoji picker to scroll and check if more GIFs need to be
          // appended. Not possible to use waitForCondition here as nothing is
          // actually supposed to change.
          await timeout(400);

          const gifResults2 = findInEmojiPicker(subcategoryGroupSelector(
              category,
              emojiPicker.activeInfiniteGroupId!,
              ))!.shadowRoot!.querySelectorAll('emoji-image');
          assertEquals(gifResults2.length, 6);
        });

    test(
        'More GIFs are loaded when user scrolls down far enough.', async () => {
          const categoryButton =
              findInEmojiPicker('emoji-search')!.shadowRoot!
                  .querySelectorAll('emoji-category-button')[categoryIndex]!
                  .shadowRoot!.querySelector('cr-icon-button');
          categoryButton!.click();
          flush();

          await waitForCondition(
              () => emojiPicker.activeInfiniteGroupId === TRENDING_GROUP_ID,
              'Wait for trending to be active.');

          const group =
              await waitUntilFindInEmojiPicker(subcategoryGroupSelector(
                  category,
                  emojiPicker.activeInfiniteGroupId!,
                  ));

          const gifResults1 =
              group!.shadowRoot!.querySelectorAll('emoji-image');
          assertEquals(gifResults1.length, 6);

          scrollToBottom();

          await waitForCondition(
              () =>
                  group?.shadowRoot?.querySelectorAll('emoji-image').length ===
                  12,
              'Wait for emoji picker to scroll and render new gifs.');

          const gifResults2 =
              group!.shadowRoot!.querySelectorAll('emoji-image');
          assertEquals(gifResults2!.length, 12);
        });

    test('Appended GIFs are displayed in the correct order', async () => {
      const categoryButton =
          findInEmojiPicker('emoji-search')!.shadowRoot!
              .querySelectorAll('emoji-category-button')[categoryIndex]!
              .shadowRoot!.querySelector('cr-icon-button');
      categoryButton!.click();
      flush();

      // Wait for correct activeInfiniteGroupId to be set.
      await waitForCondition(
          () => emojiPicker.activeInfiniteGroupId === TRENDING_GROUP_ID,
          'Wait for trending to render.');

      const group = await waitUntilFindInEmojiPicker(subcategoryGroupSelector(
          category,
          emojiPicker.activeInfiniteGroupId!,
          ));

      const gifResults1 = group!.shadowRoot!.querySelectorAll('emoji-image');
      assertEquals(gifResults1.length, 6);

      scrollToBottom();

      // Wait for Emoji Picker to scroll and render new GIFs.
      await waitForCondition(
          () =>
              group?.shadowRoot?.querySelectorAll('emoji-image').length === 12,
          'Wait for Emoji Picker to scroll and render new GIFs.');

      const gifResults2 = group!.shadowRoot!.querySelectorAll('emoji-image');
      assertEquals(gifResults2.length, 12);

      // Check display is correct.
      const leftColResults =
          group!.shadowRoot!.querySelectorAll<HTMLImageElement>(
              'div.left-column > emoji-image');
      assertEquals(leftColResults.length, 6);
      assertEmojiImageAlt(leftColResults[0], 'Trending Left 1');
      assertEmojiImageAlt(leftColResults[1], 'Trending Left 2');
      assertEmojiImageAlt(leftColResults[2], 'Trending Left 3');
      assertEmojiImageAlt(leftColResults[3], 'Trending Left 4');
      assertEmojiImageAlt(leftColResults[4], 'Trending Left 5');
      assertEmojiImageAlt(leftColResults[5], 'Trending Left 6');

      const rightColResults =
          group!.shadowRoot!.querySelectorAll<HTMLImageElement>(
              'div.right-column > emoji-image');
      assertEquals(rightColResults.length, 6);
      assertEmojiImageAlt(rightColResults[0], 'Trending Right 1');
      assertEmojiImageAlt(rightColResults[1], 'Trending Right 2');
      assertEmojiImageAlt(rightColResults[2], 'Trending Right 3');
      assertEmojiImageAlt(rightColResults[3], 'Trending Right 4');
      assertEmojiImageAlt(rightColResults[4], 'Trending Right 5');
      assertEmojiImageAlt(rightColResults[5], 'Trending Right 6');
    });

    test('New GIFs are loaded when swapping categories', async () => {
      const categoryButton =
          findInEmojiPicker('emoji-search')!.shadowRoot!
              .querySelectorAll('emoji-category-button')[categoryIndex]!
              .shadowRoot!.querySelector('cr-icon-button');
      categoryButton!.click();
      flush();

      await waitForCondition(
          () => emojiPicker.activeInfiniteGroupId === TRENDING_GROUP_ID,
          'wait for trending to be active');

      const rightChevron = findInEmojiPicker('#right-chevron');
      await flush();
      rightChevron!.click();
      await timeout(50);

      const leftColResults = findInEmojiPicker(subcategoryGroupSelector(
          category, emojiPicker.activeInfiniteGroupId!))!.shadowRoot!
                                 .querySelectorAll<HTMLImageElement>(
                                     'div.left-column > emoji-image');
      assertEquals(leftColResults.length, 3);
      assertEmojiImageAlt(leftColResults[0], 'Left 1');
      assertEmojiImageAlt(leftColResults[1], 'Left 2');
      assertEmojiImageAlt(leftColResults[2], 'Left 3');

      const rightColResults = findInEmojiPicker(subcategoryGroupSelector(
          category, emojiPicker.activeInfiniteGroupId!))!.shadowRoot!
                                  .querySelectorAll<HTMLImageElement>(
                                      'div.right-column > emoji-image');
      assertEquals(rightColResults.length, 3);
      assertEmojiImageAlt(rightColResults[0], 'Right 1');
      assertEmojiImageAlt(rightColResults[1], 'Right 2');
      assertEmojiImageAlt(rightColResults[2], 'Right 3');
    });

    test('GIFs append correctly for non-Trending categories.', async () => {
      const categoryButton =
          findInEmojiPicker('emoji-search')!.shadowRoot!
              .querySelectorAll('emoji-category-button')[categoryIndex]!
              .shadowRoot!.querySelector('cr-icon-button');
      categoryButton!.click();
      flush();

      await waitForCondition(
          () => emojiPicker.activeInfiniteGroupId === TRENDING_GROUP_ID,
          'wait for correct group to be active');

      const rightChevron = findInEmojiPicker('#right-chevron');
      await flush();
      rightChevron!.click();
      await timeout(50);

      const gifResults1 = findInEmojiPicker(subcategoryGroupSelector(
          category,
          emojiPicker.activeInfiniteGroupId!,
          ))!.shadowRoot!.querySelectorAll('emoji-image');
      assertEquals(gifResults1.length, 6);

      scrollToBottom();

      // Wait for Emoji Picker to scroll and render new GIFs.
      await waitForCondition(
          () => findInEmojiPicker(
                    subcategoryGroupSelector(
                        category, emojiPicker.activeInfiniteGroupId!))
                    ?.shadowRoot?.querySelectorAll('emoji-image')
                    .length === 12,
          'wait for emoji picker to scroll and render new gifs');

      const gifResults2 = findInEmojiPicker(subcategoryGroupSelector(
          category,
          emojiPicker.activeInfiniteGroupId!,
          ))!.shadowRoot!.querySelectorAll('emoji-image');
      assertEquals(gifResults2.length, 12);

      const leftColResults = findInEmojiPicker(subcategoryGroupSelector(
          category, emojiPicker.activeInfiniteGroupId!))!.shadowRoot!
                                 .querySelectorAll<HTMLImageElement>(
                                     'div.left-column > emoji-image');
      assertEquals(leftColResults.length, 6);
      assertEmojiImageAlt(leftColResults[0], 'Left 1');
      assertEmojiImageAlt(leftColResults[1], 'Left 2');
      assertEmojiImageAlt(leftColResults[2], 'Left 3');
      assertEmojiImageAlt(leftColResults[3], 'Left 4');
      assertEmojiImageAlt(leftColResults[4], 'Left 5');
      assertEmojiImageAlt(leftColResults[5], 'Left 6');

      const rightColResults = findInEmojiPicker(subcategoryGroupSelector(
          category, emojiPicker.activeInfiniteGroupId!))!.shadowRoot!
                                  .querySelectorAll<HTMLImageElement>(
                                      'div.right-column > emoji-image');
      assertEquals(rightColResults!.length, 6);
      assertEmojiImageAlt(rightColResults[0], 'Right 1');
      assertEmojiImageAlt(rightColResults[1], 'Right 2');
      assertEmojiImageAlt(rightColResults[2], 'Right 3');
      assertEmojiImageAlt(rightColResults[3], 'Right 4');
      assertEmojiImageAlt(rightColResults[4], 'Right 5');
      assertEmojiImageAlt(rightColResults[5], 'Right 6');
    });
  });
}