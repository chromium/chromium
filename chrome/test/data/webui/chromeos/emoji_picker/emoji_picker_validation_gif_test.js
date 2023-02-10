// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GIF_VALIDATION_DATE, TRENDING} from 'chrome://emoji-picker/constants.js';
import {EmojiPicker} from 'chrome://emoji-picker/emoji_picker.js';
import {EmojiPickerApiProxyImpl} from 'chrome://emoji-picker/emoji_picker_api_proxy.js';
import {EMOJI_PICKER_READY} from 'chrome://emoji-picker/events.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {deepQuerySelector, waitForCondition} from './emoji_picker_test_util.js';
import {TestEmojiPickerApiProxyImpl} from './test_emoji_picker_api_proxy.js';

function historyGroupSelector(category) {
  return `[data-group="${category}-history"] > ` +
      `emoji-group[category="${category}"]`;
}

function subcategoryGroupSelector(category, subcategory) {
  return `[data-group="${subcategory}"] > ` +
      `emoji-group[category="${category}"]`;
}

suite(`emoji-picker-validation-gif`, () => {
  /** @type {!EmojiPicker} */
  let emojiPicker;
  /** @type {function(...!string): ?HTMLElement} */
  let findInEmojiPicker;
  /** @type {function(string): void} */
  let scrollToBottom;
  /** @type {Array<string>} */
  const categoryList = ['emoji', 'symbol', 'emoticon', 'gif'];
  /** @type {number} */
  let categoryIndex;

  setup(() => {
    // Reset DOM state.
    document.body.innerHTML = '';
    window.localStorage.clear();
    window.localStorage.setItem(GIF_VALIDATION_DATE, new Date(0).toJSON());
    const oneByOneGif =
        'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=';
    const oneByTwoGif =
        'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAACCAQAAAAziH6sAAAADklEQVR42mNk+M/I8B8ABQoCAV5AcKEAAAAASUVORK5CYII=';

    // The name left of - is the position of the GIF initially.
    // The name right of - is the position of the GIF after history GIFs have
    // been validated.
    const historyGifs = {
      history: [
        {
          base: {
            visualContent: {
              id: '0',
              url: {
                full: {
                  url: oneByTwoGif,
                },
                preview: {
                  url: oneByTwoGif,
                },
              },
              previewSize: {width: 1, height: 2},
            },
            name: 'Left 1 - Invalid',
          },
          alternates: [],
        },
        {
          base: {
            visualContent: {
              id: '1',
              url: {
                full: {
                  url: oneByOneGif,
                },
                preview: {
                  url: oneByOneGif,
                },
              },
              previewSize: {width: 1, height: 1},
            },
            name: 'Right 1 - Left 1',
          },
          alternates: [],
        },
        {
          base: {
            visualContent: {
              id: '2',
              url: {
                full: {
                  url: oneByTwoGif,
                },
                preview: {
                  url: oneByTwoGif,
                },
              },
              previewSize: {width: 1, height: 2},
            },
            name: 'Right 2 - Right 1',
          },
          alternates: [],
        },
        {
          base: {
            visualContent: {
              id: 3,
              url: {
                full: {
                  url: oneByTwoGif,
                },
                preview: {
                  url: oneByTwoGif,
                },
              },
              previewSize: {width: 1, height: 2},
            },
            name: 'Left 2 - Invalid',
          },
          alternates: [],
        },
        {
          base: {
            visualContent: {
              id: '4',
              url: {
                full: {
                  url: oneByTwoGif,
                },
                preview: {
                  url: oneByTwoGif,
                },
              },
              previewSize: {width: 1, height: 2},
            },
            name: 'Right 3 - Left 2',
          },
          alternates: [],
        },
      ],
      preference: {},
    };

    // Set GIF history.
    window.localStorage.setItem(
        'gif-recently-used', JSON.stringify(historyGifs));

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

    categoryIndex = categoryList.indexOf('gif');

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
      `recently used gif group should contain the ` +
          `correct gifs after it is has been validated.`,
      async () => {
        emojiPicker.updateIncognitoState(false);

        // Whilst history originally had 5 GIFs, there should now only be 3
        // valid GIFs.
        const recentlyUsedEmoji =
            findInEmojiPicker(historyGroupSelector('gif'))
                .shadowRoot.querySelectorAll('.emoji-button');

        assertEquals(3, recentlyUsedEmoji.length);

        // Check display is correct and the correct GIFs have been deleted.
        const leftColResults =
            findInEmojiPicker(historyGroupSelector('gif'))
                .shadowRoot.querySelectorAll('div.left-column > .emoji-button');
        assertEquals(leftColResults.length, 2);
        assert(leftColResults[0].alt === 'Right 1 - Left 1');
        assert(leftColResults[1].alt === 'Right 3 - Left 2');

        const rightColResults = findInEmojiPicker(historyGroupSelector('gif'))
                                    .shadowRoot.querySelectorAll(
                                        'div.right-column > .emoji-button');
        assertEquals(rightColResults.length, 1);
        assert(rightColResults[0].alt === 'Right 2 - Right 1');
      });

  test(
      'Trending appends GIFs correctly via scrolling when' +
          ' recently used group exists',
      async () => {
        emojiPicker.updateIncognitoState(false);

        const categoryButton =
            findInEmojiPicker('emoji-search')
                .shadowRoot
                .querySelectorAll('emoji-category-button')[categoryIndex]
                .shadowRoot.querySelector('cr-icon-button');
        categoryButton.click();
        flush();

        const recentlyUsedEmoji =
            findInEmojiPicker(historyGroupSelector('gif'))
                .shadowRoot.querySelectorAll('.emoji-button');

        assertEquals(3, recentlyUsedEmoji.length);

        const trendingId = emojiPicker.categoriesGroupElements
                               .find(group => group.name === TRENDING)
                               ?.groupId;

        // Scroll to Trending subcategory.
        emojiPicker.shadowRoot.querySelector(`div[data-group="${trendingId}"]`)
            ?.scrollIntoView();

        await waitForCondition(
            () => emojiPicker.activeInfiniteGroupId === trendingId);

        const gifResults1 =
            findInEmojiPicker(subcategoryGroupSelector(
                                  'gif', emojiPicker.activeInfiniteGroupId))
                .shadowRoot.querySelectorAll('.emoji-button');
        assertEquals(gifResults1.length, 6);

        // Check display is correct.
        const leftColResults1 =
            findInEmojiPicker(subcategoryGroupSelector(
                                  'gif', emojiPicker.activeInfiniteGroupId))
                .shadowRoot.querySelectorAll('div.left-column > .emoji-button');
        assertEquals(leftColResults1.length, 3);
        assert(leftColResults1[0].alt === 'Trending Left 1');
        assert(leftColResults1[1].alt === 'Trending Left 2');
        assert(leftColResults1[2].alt === 'Trending Left 3');

        const rightColResults1 =
            findInEmojiPicker(subcategoryGroupSelector(
                                  'gif', emojiPicker.activeInfiniteGroupId))
                .shadowRoot.querySelectorAll(
                    'div.right-column > .emoji-button');
        assertEquals(rightColResults1.length, 3);
        assert(rightColResults1[0].alt === 'Trending Right 1');
        assert(rightColResults1[1].alt === 'Trending Right 2');
        assert(rightColResults1[2].alt === 'Trending Right 3');

        scrollToBottom();

        // Wait for Emoji Picker to scroll and render new GIFs.
        await waitForCondition(
            () =>
                findInEmojiPicker(subcategoryGroupSelector(
                                      'gif', emojiPicker.activeInfiniteGroupId))
                    .shadowRoot.querySelectorAll('.emoji-button')
                    .length === 12);

        const gifResults2 =
            findInEmojiPicker(subcategoryGroupSelector(
                                  'gif', emojiPicker.activeInfiniteGroupId))
                .shadowRoot.querySelectorAll('.emoji-button');
        assertEquals(gifResults2.length, 12);

        // Check display is correct.
        const leftColResults2 =
            findInEmojiPicker(subcategoryGroupSelector(
                                  'gif', emojiPicker.activeInfiniteGroupId))
                .shadowRoot.querySelectorAll('div.left-column > .emoji-button');
        assertEquals(leftColResults2.length, 6);
        assert(leftColResults2[0].alt === 'Trending Left 1');
        assert(leftColResults2[1].alt === 'Trending Left 2');
        assert(leftColResults2[2].alt === 'Trending Left 3');
        assert(leftColResults2[3].alt === 'Trending Left 4');
        assert(leftColResults2[4].alt === 'Trending Left 5');
        assert(leftColResults2[5].alt === 'Trending Left 6');

        const rightColResults2 =
            findInEmojiPicker(subcategoryGroupSelector(
                                  'gif', emojiPicker.activeInfiniteGroupId))
                .shadowRoot.querySelectorAll(
                    'div.right-column > .emoji-button');
        assertEquals(rightColResults2.length, 6);
        assert(rightColResults2[0].alt === 'Trending Right 1');
        assert(rightColResults2[1].alt === 'Trending Right 2');
        assert(rightColResults2[2].alt === 'Trending Right 3');
        assert(rightColResults2[3].alt === 'Trending Right 4');
        assert(rightColResults2[4].alt === 'Trending Right 5');
        assert(rightColResults2[5].alt === 'Trending Right 6');
      });

  test(
      'Trending appends GIFs correctly via selecting group' +
          ' when recently used group exists',
      async () => {
        emojiPicker.updateIncognitoState(false);

        const categoryButton =
            findInEmojiPicker('emoji-search')
                .shadowRoot
                .querySelectorAll('emoji-category-button')[categoryIndex]
                .shadowRoot.querySelector('cr-icon-button');
        categoryButton.click();
        flush();

        const recentlyUsedEmoji =
            findInEmojiPicker(historyGroupSelector('gif'))
                .shadowRoot.querySelectorAll('.emoji-button');

        assertEquals(3, recentlyUsedEmoji.length);

        const trendingId = emojiPicker.categoriesGroupElements
                               .find(group => group.name === TRENDING)
                               ?.groupId;

        const trendingSubcategoryButton =
            emojiPicker.shadowRoot
                .querySelector(
                    `#tabs text-group-button[data-group="${trendingId}"]`)
                .shadowRoot.querySelector('cr-button');
        trendingSubcategoryButton.click();
        await flush();

        const gifResults1 =
            findInEmojiPicker(subcategoryGroupSelector(
                                  'gif', emojiPicker.activeInfiniteGroupId))
                .shadowRoot.querySelectorAll('.emoji-button');
        assertEquals(gifResults1.length, 6);

        // Check display is correct.
        const leftColResults1 =
            findInEmojiPicker(subcategoryGroupSelector(
                                  'gif', emojiPicker.activeInfiniteGroupId))
                .shadowRoot.querySelectorAll('div.left-column > .emoji-button');
        assertEquals(leftColResults1.length, 3);
        assert(leftColResults1[0].alt === 'Trending Left 1');
        assert(leftColResults1[1].alt === 'Trending Left 2');
        assert(leftColResults1[2].alt === 'Trending Left 3');

        const rightColResults1 =
            findInEmojiPicker(subcategoryGroupSelector(
                                  'gif', emojiPicker.activeInfiniteGroupId))
                .shadowRoot.querySelectorAll(
                    'div.right-column > .emoji-button');
        assertEquals(rightColResults1.length, 3);
        assert(rightColResults1[0].alt === 'Trending Right 1');
        assert(rightColResults1[1].alt === 'Trending Right 2');
        assert(rightColResults1[2].alt === 'Trending Right 3');

        scrollToBottom();

        // Wait for Emoji Picker to scroll and render new GIFs.
        await waitForCondition(
            () =>
                findInEmojiPicker(subcategoryGroupSelector(
                                      'gif', emojiPicker.activeInfiniteGroupId))
                    .shadowRoot.querySelectorAll('.emoji-button')
                    .length === 12);

        const gifResults2 =
            findInEmojiPicker(subcategoryGroupSelector(
                                  'gif', emojiPicker.activeInfiniteGroupId))
                .shadowRoot.querySelectorAll('.emoji-button');
        assertEquals(gifResults2.length, 12);

        // Check display is correct.
        const leftColResults2 =
            findInEmojiPicker(subcategoryGroupSelector(
                                  'gif', emojiPicker.activeInfiniteGroupId))
                .shadowRoot.querySelectorAll('div.left-column > .emoji-button');
        assertEquals(leftColResults2.length, 6);
        assert(leftColResults2[0].alt === 'Trending Left 1');
        assert(leftColResults2[1].alt === 'Trending Left 2');
        assert(leftColResults2[2].alt === 'Trending Left 3');
        assert(leftColResults2[3].alt === 'Trending Left 4');
        assert(leftColResults2[4].alt === 'Trending Left 5');
        assert(leftColResults2[5].alt === 'Trending Left 6');

        const rightColResults2 =
            findInEmojiPicker(subcategoryGroupSelector(
                                  'gif', emojiPicker.activeInfiniteGroupId))
                .shadowRoot.querySelectorAll(
                    'div.right-column > .emoji-button');
        assertEquals(rightColResults2.length, 6);
        assert(rightColResults2[0].alt === 'Trending Right 1');
        assert(rightColResults2[1].alt === 'Trending Right 2');
        assert(rightColResults2[2].alt === 'Trending Right 3');
        assert(rightColResults2[3].alt === 'Trending Right 4');
        assert(rightColResults2[4].alt === 'Trending Right 5');
        assert(rightColResults2[5].alt === 'Trending Right 6');
      });
});