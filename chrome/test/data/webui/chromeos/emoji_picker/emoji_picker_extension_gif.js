// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TRENDING_GROUP_ID} from 'chrome://emoji-picker/constants.js';
import {EmojiPicker} from 'chrome://emoji-picker/emoji_picker.js';
import {EmojiPickerApiProxyImpl} from 'chrome://emoji-picker/emoji_picker_api_proxy.js';
import {EMOJI_IMG_BUTTON_CLICK, EMOJI_PICKER_READY} from 'chrome://emoji-picker/events.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {deepQuerySelector, isGroupButtonActive, timeout, waitForCondition, waitWithTimeout} from './emoji_picker_test_util.js';
import {TestEmojiPickerApiProxyImpl} from './test_emoji_picker_api_proxy.js';

const ACTIVE_CATEGORY_BUTTON = 'category-button-active';

function isCategoryButtonActive(element) {
  assert(element, 'category button element should not be null.');
  return element.classList.contains(ACTIVE_CATEGORY_BUTTON);
}

function categoryGroupSelector(category) {
  return `emoji-group[category="${category}"]:not(.history)`;
}

function historyGroupSelector(category) {
  return `[data-group="${category}-history"] > ` +
      `emoji-group[category="${category}"]`;
}

function subcategoryGroupSelector(category, subcategory) {
  return `[data-group="${subcategory}"] > ` +
      `emoji-group[category="${category}"]`;
}

export function GifTestSuite(category) {
  suite(`emoji-picker-extension-${category}`, () => {
    /** @type {!EmojiPicker} */
    let emojiPicker;
    /** @type {function(...!string): ?HTMLElement} */
    let findInEmojiPicker;
    /** @type {function(...!string): ?HTMLElement} */
    let findEmojiFirstButton;
    /** @type {function(string): void} */
    let scrollDown;
    /** @type {function(): void} */
    let scrollToBottom;
    /** @type {Array<string>} */
    const categoryList = ['emoji', 'symbol', 'emoticon', 'gif'];
    /** @type {number} */
    let categoryIndex;

    setup(() => {
      // Reset DOM state.
      document.body.innerHTML = '';
      window.localStorage.clear();

      EmojiPickerApiProxyImpl.setInstance(new TestEmojiPickerApiProxyImpl());

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
          'gif': [],
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

      scrollDown = (height) => {
        const thisRect = emojiPicker.$['groups'];
        if (thisRect) {
          thisRect.scrollTop += height;
        }
      };

      scrollToBottom = () => {
        const thisRect = emojiPicker.$['groups'];
        if (!thisRect) {
          return;
        }
        const searchResultRect =
            emojiPicker.getActiveGroupAndId(thisRect.getBoundingClientRect())
                .group;
        if (searchResultRect) {
          thisRect.scrollTop += searchResultRect.getBoundingClientRect().bottom;
        }
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
        async () => {
          const allCategoryButtons =
              Array
                  .from(
                      findInEmojiPicker('emoji-search')
                          .shadowRoot.querySelectorAll('emoji-category-button')
                          .values())
                  .map(item => item.shadowRoot.querySelector('cr-icon-button'));
          const categoryButton = allCategoryButtons[categoryIndex];
          categoryButton.click();
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
        () => {
          // It is assumed that the order of categoryList is the same as
          // buttons.
          const categoryButton =
              findInEmojiPicker('emoji-search')
                  .shadowRoot
                  .querySelectorAll('emoji-category-button')[categoryIndex]
                  .shadowRoot.querySelector('cr-icon-button');
          categoryButton.click();
          flush();
          const historyTab = findInEmojiPicker(
              `#tabs emoji-group-button[data-group="${category}-history"]`,
              'cr-icon-button');
          assertTrue(historyTab.disabled);
        });

    test(
        `clicking at a ${category} button should trigger the clicking event` +
            `correct ${category} string and name.`,
        async () => {
          const firstButton = await waitForCondition(
              () => findEmojiFirstButton(categoryGroupSelector(category)));
          const groupElements = emojiPicker.categoriesGroupElements.filter(
              item => item.category === category && !item.isHistory);
          const expectedString =
              groupElements[0].emoji[0].base.visualContent.url.preview;
          const expectedName = groupElements[0].emoji[0].base.name;
          const buttonClickPromise = new Promise(
              (resolve) => emojiPicker.addEventListener(
                  EMOJI_IMG_BUTTON_CLICK, (event) => {
                    assertEquals(
                        expectedString, event.detail.visualContent.url.preview);
                    assertEquals(expectedName, event.detail.name);
                    resolve();
                  }));
          firstButton.click();
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
          emojiButton.click();

          const recentEmojiButton = await waitForCondition(
              () => findEmojiFirstButton(historyGroupSelector(category)));
          assert(recentEmojiButton);

          const recentlyUsedEmoji =
              findInEmojiPicker(historyGroupSelector(category))
                  .shadowRoot.querySelectorAll('.emoji-button');
          assertEquals(1, recentlyUsedEmoji.length);
        });

    test(
        `recently used ${category} group should not contain ` +
            `duplicate ${category}s.`,
        async () => {
          emojiPicker.updateIncognitoState(false);

          const emojiButton =
              findEmojiFirstButton(categoryGroupSelector(category));
          emojiButton.click();

          const recentEmojiButton = await waitForCondition(
              () => findEmojiFirstButton(historyGroupSelector(category)));
          assert(recentEmojiButton);

          const recentlyUsedEmoji1 =
              findInEmojiPicker(historyGroupSelector(category))
                  .shadowRoot.querySelectorAll('.emoji-button');
          assertEquals(1, recentlyUsedEmoji1.length);

          // Click the same emoji again
          emojiButton.click();

          const recentlyUsedEmoji2 =
              findInEmojiPicker(historyGroupSelector(category))
                  .shadowRoot.querySelectorAll('.emoji-button');
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
          gifCategoryButton.click();
          await flush();
          const firstGifTabInFirstPage =
              findInEmojiPicker('.pagination text-group-button', 'cr-button');
          const firstGifTabInSecondPage = findInEmojiPicker(
              '.pagination + .pagination', 'text-group-button', 'cr-button');
          rightChevron.click();

          await flush();
          await waitForCondition(
              () => !isGroupButtonActive(firstGifTabInFirstPage) &&
                  isGroupButtonActive(firstGifTabInSecondPage));
          leftChevron.click();
          await flush();
          await waitForCondition(
              () => !isGroupButtonActive(firstGifTabInSecondPage) &&
                  isGroupButtonActive(firstGifTabInFirstPage));
        });

    test('Trending should display GIFs.', async () => {
      const categoryButton =
          findInEmojiPicker('emoji-search')
              .shadowRoot
              .querySelectorAll('emoji-category-button')[categoryIndex]
              .shadowRoot.querySelector('cr-icon-button');
      categoryButton.click();
      flush();

      // Wait for correct activeInfiniteGroupId to be set.
      await waitForCondition(
          () => emojiPicker.activeInfiniteGroupId === TRENDING_GROUP_ID);

      const gifResults =
          findInEmojiPicker(subcategoryGroupSelector(
                                category, emojiPicker.activeInfiniteGroupId))
              .shadowRoot.querySelectorAll('.emoji-button');
      assertEquals(gifResults.length, 6);
    });

    test('Trending should display GIFs in the correct order.', async () => {
      const categoryButton =
          findInEmojiPicker('emoji-search')
              .shadowRoot
              .querySelectorAll('emoji-category-button')[categoryIndex]
              .shadowRoot.querySelector('cr-icon-button');
      categoryButton.click();
      flush();

      // Wait for correct activeInfiniteGroupId to be set.
      await waitForCondition(
          () => emojiPicker.activeInfiniteGroupId === TRENDING_GROUP_ID);

      const gifResults =
          findInEmojiPicker(subcategoryGroupSelector(
                                category, emojiPicker.activeInfiniteGroupId))
              .shadowRoot.querySelectorAll('.emoji-button');
      assertEquals(gifResults.length, 6);

      // Check display is correct.
      const leftColResults =
          findInEmojiPicker(subcategoryGroupSelector(
                                category, emojiPicker.activeInfiniteGroupId))
              .shadowRoot.querySelectorAll('div.left-column > .emoji-button');
      assertEquals(leftColResults.length, 3);
      assert(leftColResults[0].alt === 'Trending Left 1');
      assert(leftColResults[1].alt === 'Trending Left 2');
      assert(leftColResults[2].alt === 'Trending Left 3');

      const rightColResults =
          findInEmojiPicker(subcategoryGroupSelector(
                                category, emojiPicker.activeInfiniteGroupId))
              .shadowRoot.querySelectorAll('div.right-column > .emoji-button');
      assertEquals(rightColResults.length, 3);
      assert(rightColResults[0].alt === 'Trending Right 1');
      assert(rightColResults[1].alt === 'Trending Right 2');
      assert(rightColResults[2].alt === 'Trending Right 3');
    });

    test(
        'User does not load more GIFs if they have not scrolled down' +
            ' far enough',
        async () => {
          const categoryButton =
              findInEmojiPicker('emoji-search')
                  .shadowRoot
                  .querySelectorAll('emoji-category-button')[categoryIndex]
                  .shadowRoot.querySelector('cr-icon-button');
          categoryButton.click();
          flush();

          // Wait for correct activeInfiniteGroupId to be set.
          await waitForCondition(
              () => emojiPicker.activeInfiniteGroupId === TRENDING_GROUP_ID);

          const gifResults1 =
              findInEmojiPicker(
                  subcategoryGroupSelector(
                      category, emojiPicker.activeInfiniteGroupId))
                  .shadowRoot.querySelectorAll('.emoji-button');
          assertEquals(gifResults1.length, 6);

          // Scroll down a little bit to activate checking if we need more GIFs.
          scrollDown(100);

          // Wait for emoji picker to scroll and check if more GIFs need to be
          // appended. Not possible to use waitForCondition here as nothing is
          // actually supposed to change.
          await timeout(400);

          const gifResults2 =
              findInEmojiPicker(
                  subcategoryGroupSelector(
                      category, emojiPicker.activeInfiniteGroupId))
                  .shadowRoot.querySelectorAll('.emoji-button');
          assertEquals(gifResults2.length, 6);
        });

    test(
        'More GIFs are loaded when user scrolls down far enough.', async () => {
          const categoryButton =
              findInEmojiPicker('emoji-search')
                  .shadowRoot
                  .querySelectorAll('emoji-category-button')[categoryIndex]
                  .shadowRoot.querySelector('cr-icon-button');
          categoryButton.click();
          flush();

          // Wait for correct activeInfiniteGroupId to be set.
          await waitForCondition(
              () => emojiPicker.activeInfiniteGroupId === TRENDING_GROUP_ID);

          const gifResults1 =
              findInEmojiPicker(
                  subcategoryGroupSelector(
                      category, emojiPicker.activeInfiniteGroupId))
                  .shadowRoot.querySelectorAll('.emoji-button');
          assertEquals(gifResults1.length, 6);

          scrollToBottom();

          // Wait for Emoji Picker to scroll and render new GIFs.
          await waitForCondition(
              () => findInEmojiPicker(
                        subcategoryGroupSelector(
                            category, emojiPicker.activeInfiniteGroupId))
                        .shadowRoot.querySelectorAll('.emoji-button')
                        .length === 12);

          const gifResults2 =
              findInEmojiPicker(
                  subcategoryGroupSelector(
                      category, emojiPicker.activeInfiniteGroupId))
                  .shadowRoot.querySelectorAll('.emoji-button');
          assertEquals(gifResults2.length, 12);
        });

    test('Appended GIFs are displayed in the correct order', async () => {
      const categoryButton =
          findInEmojiPicker('emoji-search')
              .shadowRoot
              .querySelectorAll('emoji-category-button')[categoryIndex]
              .shadowRoot.querySelector('cr-icon-button');
      categoryButton.click();
      flush();

      // Wait for correct activeInfiniteGroupId to be set.
      await waitForCondition(
          () => emojiPicker.activeInfiniteGroupId === TRENDING_GROUP_ID);

      const gifResults1 =
          findInEmojiPicker(subcategoryGroupSelector(
                                category, emojiPicker.activeInfiniteGroupId))
              .shadowRoot.querySelectorAll('.emoji-button');
      assertEquals(gifResults1.length, 6);

      scrollToBottom();

      // Wait for Emoji Picker to scroll and render new GIFs.
      await waitForCondition(
          () => findInEmojiPicker(
                    subcategoryGroupSelector(
                        category, emojiPicker.activeInfiniteGroupId))
                    .shadowRoot.querySelectorAll('.emoji-button')
                    .length === 12);

      const gifResults2 =
          findInEmojiPicker(subcategoryGroupSelector(
                                category, emojiPicker.activeInfiniteGroupId))
              .shadowRoot.querySelectorAll('.emoji-button');
      assertEquals(gifResults2.length, 12);

      // Check display is correct.
      const leftColResults =
          findInEmojiPicker(subcategoryGroupSelector(
                                category, emojiPicker.activeInfiniteGroupId))
              .shadowRoot.querySelectorAll('div.left-column > .emoji-button');
      assertEquals(leftColResults.length, 6);
      assert(leftColResults[0].alt === 'Trending Left 1');
      assert(leftColResults[1].alt === 'Trending Left 2');
      assert(leftColResults[2].alt === 'Trending Left 3');
      assert(leftColResults[3].alt === 'Trending Left 4');
      assert(leftColResults[4].alt === 'Trending Left 5');
      assert(leftColResults[5].alt === 'Trending Left 6');

      const rightColResults =
          findInEmojiPicker(subcategoryGroupSelector(
                                category, emojiPicker.activeInfiniteGroupId))
              .shadowRoot.querySelectorAll('div.right-column > .emoji-button');
      assertEquals(rightColResults.length, 6);
      assert(rightColResults[0].alt === 'Trending Right 1');
      assert(rightColResults[1].alt === 'Trending Right 2');
      assert(rightColResults[2].alt === 'Trending Right 3');
      assert(rightColResults[3].alt === 'Trending Right 4');
      assert(rightColResults[4].alt === 'Trending Right 5');
      assert(rightColResults[5].alt === 'Trending Right 6');
    });

    test('New GIFs are loaded when swapping categories', async () => {
      const categoryButton =
          findInEmojiPicker('emoji-search')
              .shadowRoot
              .querySelectorAll('emoji-category-button')[categoryIndex]
              .shadowRoot.querySelector('cr-icon-button');
      categoryButton.click();
      flush();

      // Wait for correct activeInfiniteGroupId to be set.
      await waitForCondition(
          () => emojiPicker.activeInfiniteGroupId === TRENDING_GROUP_ID);

      const rightChevron = findInEmojiPicker('#right-chevron');
      await flush();
      rightChevron.click();
      await timeout(50);

      const leftColResults =
          findInEmojiPicker(subcategoryGroupSelector(
                                category, emojiPicker.activeInfiniteGroupId))
              .shadowRoot.querySelectorAll('div.left-column > .emoji-button');
      assertEquals(leftColResults.length, 3);
      assert(leftColResults[0].alt === 'Left 1');
      assert(leftColResults[1].alt === 'Left 2');
      assert(leftColResults[2].alt === 'Left 3');

      const rightColResults =
          findInEmojiPicker(subcategoryGroupSelector(
                                category, emojiPicker.activeInfiniteGroupId))
              .shadowRoot.querySelectorAll('div.right-column > .emoji-button');
      assertEquals(rightColResults.length, 3);
      assert(rightColResults[0].alt === 'Right 1');
      assert(rightColResults[1].alt === 'Right 2');
      assert(rightColResults[2].alt === 'Right 3');
    });

    test('GIFs append correctly for non-Trending categories.', async () => {
      const categoryButton =
          findInEmojiPicker('emoji-search')
              .shadowRoot
              .querySelectorAll('emoji-category-button')[categoryIndex]
              .shadowRoot.querySelector('cr-icon-button');
      categoryButton.click();
      flush();

      // Wait for correct activeInfiniteGroupId to be set.
      await waitForCondition(
          () => emojiPicker.activeInfiniteGroupId === TRENDING_GROUP_ID);

      const rightChevron = findInEmojiPicker('#right-chevron');
      await flush();
      rightChevron.click();
      await timeout(50);

      const gifResults1 =
          findInEmojiPicker(subcategoryGroupSelector(
                                category, emojiPicker.activeInfiniteGroupId))
              .shadowRoot.querySelectorAll('.emoji-button');
      assertEquals(gifResults1.length, 6);

      scrollToBottom();

      // Wait for Emoji Picker to scroll and render new GIFs.
      await waitForCondition(
          () => findInEmojiPicker(
                    subcategoryGroupSelector(
                        category, emojiPicker.activeInfiniteGroupId))
                    .shadowRoot.querySelectorAll('.emoji-button')
                    .length === 12);

      const gifResults2 =
          findInEmojiPicker(subcategoryGroupSelector(
                                category, emojiPicker.activeInfiniteGroupId))
              .shadowRoot.querySelectorAll('.emoji-button');
      assertEquals(gifResults2.length, 12);

      const leftColResults =
          findInEmojiPicker(subcategoryGroupSelector(
                                category, emojiPicker.activeInfiniteGroupId))
              .shadowRoot.querySelectorAll('div.left-column > .emoji-button');
      assertEquals(leftColResults.length, 6);
      assert(leftColResults[0].alt === 'Left 1');
      assert(leftColResults[1].alt === 'Left 2');
      assert(leftColResults[2].alt === 'Left 3');
      assert(leftColResults[3].alt === 'Left 4');
      assert(leftColResults[4].alt === 'Left 5');
      assert(leftColResults[5].alt === 'Left 6');

      const rightColResults =
          findInEmojiPicker(subcategoryGroupSelector(
                                category, emojiPicker.activeInfiniteGroupId))
              .shadowRoot.querySelectorAll('div.right-column > .emoji-button');
      assertEquals(rightColResults.length, 6);
      assert(rightColResults[0].alt === 'Right 1');
      assert(rightColResults[1].alt === 'Right 2');
      assert(rightColResults[2].alt === 'Right 3');
      assert(rightColResults[3].alt === 'Right 4');
      assert(rightColResults[4].alt === 'Right 5');
      assert(rightColResults[5].alt === 'Right 6');
    });
  });
}