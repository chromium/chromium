// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {EmojiGroupElement, EmojiPickerApiProxy, EmojiPickerApp, GIF_VALIDATION_DATE, TRENDING} from 'chrome://emoji-picker/emoji_picker.js';
import {assert} from 'chrome://resources/js/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {assertEmojiImageAlt, initialiseEmojiPickerForTest, waitForCondition} from './emoji_picker_test_util.js';
import {TestEmojiPickerApiProxy} from './test_emoji_picker_api_proxy.js';

function historyGroupSelector(category: string) {
  return `[data-group="${category}-history"] > ` +
      `emoji-group[category="${category}"]`;
}

function subcategoryGroupSelector(category: string, subcategory: string) {
  return `[data-group="${subcategory}"] > ` +
      `emoji-group[category="${category}"]`;
}

suite(`emoji-picker-validation-gif`, () => {
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

  EmojiPickerApiProxy.setInstance(new TestEmojiPickerApiProxy());

  let emojiPicker: EmojiPickerApp;
  let findInEmojiPicker: (...path: string[]) => HTMLElement | null;
  let waitUntilFindInEmojiPicker: (...path: string[]) => Promise<HTMLElement>;
  let scrollToBottom: () => void;
  const categoryList = ['emoji', 'symbol', 'emoticon', 'gif'];
  let categoryIndex: number;

  setup(async () => {
    const newPicker = initialiseEmojiPickerForTest(false, [
      {key: GIF_VALIDATION_DATE, value: new Date(0).toJSON()},
      {key: 'gif-recently-used', value: JSON.stringify(historyGifs)},
    ]);
    emojiPicker = newPicker.emojiPicker;
    findInEmojiPicker = newPicker.findInEmojiPicker;
    waitUntilFindInEmojiPicker = newPicker.waitUntilFindInEmojiPicker;
    const readyPromise = newPicker.readyPromise;
    scrollToBottom = newPicker.scrollToBottom;
    await readyPromise;

    categoryIndex = categoryList.indexOf('gif');
  });


  test(
      `recently used gif group should contain the ` +
          `correct gifs after it is has been validated.`,
      async () => {
        await emojiPicker.updateIncognitoState(false);

        // Whilst history originally had 5 GIFs, there should now only be 3
        // valid GIFs.
        const recentlyUsedEmoji = findInEmojiPicker(historyGroupSelector(
            'gif'))!.shadowRoot!.querySelectorAll('emoji-image');

        assertEquals(3, recentlyUsedEmoji.length);

        // Check display is correct and the correct GIFs have been deleted.
        const leftColResults =
            findInEmojiPicker(historyGroupSelector('gif'))!.shadowRoot!
                .querySelectorAll<HTMLImageElement>(
                    'div.left-column > emoji-image');
        assertEquals(leftColResults.length, 2);
        assertEmojiImageAlt(leftColResults[0], 'Right 1 - Left 1');
        assertEmojiImageAlt(leftColResults[1], 'Right 3 - Left 2');

        const rightColResults =
            findInEmojiPicker(historyGroupSelector('gif'))!.shadowRoot!
                .querySelectorAll<HTMLImageElement>(
                    'div.right-column > emoji-image');
        assertEquals(rightColResults.length, 1);
        assert(rightColResults[0], 'Right 2 - Right 1');
      });

  test(
      'Trending appends GIFs correctly via scrolling when' +
          ' recently used group exists',
      async () => {
        await emojiPicker.updateIncognitoState(false);

        const categoryButton =
            findInEmojiPicker('emoji-search')!.shadowRoot!
                .querySelectorAll('emoji-category-button')
                ?.[categoryIndex]!.shadowRoot!.querySelector('cr-icon-button')!;
        categoryButton.click();
        flush();

        const recentlyUsedEmoji =
            (await waitUntilFindInEmojiPicker(historyGroupSelector(
                'gif')))!.shadowRoot!.querySelectorAll('emoji-image');

        assertEquals(3, recentlyUsedEmoji.length);

        const trendingId =
            emojiPicker.categoriesGroupElements
                .find((group: EmojiGroupElement) => group.name === TRENDING)
                ?.groupId;

        // Scroll to Trending subcategory.
        emojiPicker.shadowRoot!
            .querySelector(`div[data-group="${trendingId}"]`)
            ?.scrollIntoView();

        await waitForCondition(
            () => emojiPicker.activeInfiniteGroupId === trendingId,
            'wait for new group to be active');

        const group = await waitUntilFindInEmojiPicker(subcategoryGroupSelector(
            'gif',
            emojiPicker.activeInfiniteGroupId!,
            ));

        const gifResults1 = group!.shadowRoot!.querySelectorAll('emoji-image');
        assertEquals(gifResults1.length, 6);

        // Check display is correct.
        const leftColResults1 =
            group!.shadowRoot!.querySelectorAll<HTMLImageElement>(
                'div.left-column > emoji-image');
        assertEquals(leftColResults1.length, 3);
        assertEmojiImageAlt(leftColResults1[0], 'Trending Left 1');
        assertEmojiImageAlt(leftColResults1[1], 'Trending Left 2');
        assertEmojiImageAlt(leftColResults1[2], 'Trending Left 3');

        const rightColResults1 =
            group!.shadowRoot!.querySelectorAll<HTMLImageElement>(
                'div.right-column > emoji-image');
        assertEquals(rightColResults1.length, 3);
        assertEmojiImageAlt(rightColResults1[0], 'Trending Right 1');
        assertEmojiImageAlt(rightColResults1[1], 'Trending Right 2');
        assertEmojiImageAlt(rightColResults1[2], 'Trending Right 3');

        scrollToBottom();

        await waitForCondition(
            () => group?.shadowRoot?.querySelectorAll('emoji-image').length ===
                12,
            'wait for emoji picker to scroll and render new Gifs');

        const gifResults2 = group!.shadowRoot!.querySelectorAll('emoji-image');
        assertEquals(gifResults2.length, 12);

        // Check display is correct.
        const leftColResults2 =
            group!.shadowRoot!.querySelectorAll<HTMLImageElement>(
                'div.left-column > emoji-image');
        assertEquals(leftColResults2.length, 6);
        assertEmojiImageAlt(leftColResults2[0], 'Trending Left 1');
        assertEmojiImageAlt(leftColResults2[1], 'Trending Left 2');
        assertEmojiImageAlt(leftColResults2[2], 'Trending Left 3');
        assertEmojiImageAlt(leftColResults2[3], 'Trending Left 4');
        assertEmojiImageAlt(leftColResults2[4], 'Trending Left 5');
        assertEmojiImageAlt(leftColResults2[5], 'Trending Left 6');

        const rightColResults2 =
            group!.shadowRoot!.querySelectorAll<HTMLImageElement>(
                'div.right-column > emoji-image');
        assertEquals(rightColResults2.length, 6);
        assertEmojiImageAlt(rightColResults2[0], 'Trending Right 1');
        assertEmojiImageAlt(rightColResults2[1], 'Trending Right 2');
        assertEmojiImageAlt(rightColResults2[2], 'Trending Right 3');
        assertEmojiImageAlt(rightColResults2[3], 'Trending Right 4');
        assertEmojiImageAlt(rightColResults2[4], 'Trending Right 5');
        assertEmojiImageAlt(rightColResults2[5], 'Trending Right 6');
      });

  test(
      'Trending appends GIFs correctly via selecting group' +
          ' when recently used group exists',
      async () => {
        await emojiPicker.updateIncognitoState(false);

        const categoryButton =
            findInEmojiPicker('emoji-search')!.shadowRoot!
                .querySelectorAll('emoji-category-button')
                ?.[categoryIndex]!.shadowRoot!.querySelector<HTMLElement>(
                    'cr-icon-button');
        categoryButton!.click();
        flush();

        const recentlyUsedEmoji =
            (await waitUntilFindInEmojiPicker(historyGroupSelector(
                'gif')))!.shadowRoot!.querySelectorAll('emoji-image');

        assertEquals(3, recentlyUsedEmoji.length);

        const trendingId =
            emojiPicker.categoriesGroupElements
                .find((group: EmojiGroupElement) => group.name === TRENDING)
                ?.groupId;

        const trendingSubcategoryButton = (await waitUntilFindInEmojiPicker(
            `#tabs text-group-button[data-group="${
                trendingId}"]`))!.shadowRoot!.querySelector('cr-button');

        trendingSubcategoryButton!.click();
        await flush();

        await waitForCondition(
            () => emojiPicker.activeInfiniteGroupId === trendingId,
            'wait for new group to be active');

        const group = await waitUntilFindInEmojiPicker(subcategoryGroupSelector(
            'gif',
            emojiPicker.activeInfiniteGroupId!,
            ));

        await waitForCondition(
            () =>
                group!.shadowRoot!.querySelectorAll('emoji-image').length === 6,
            'wait for trending GIFs');

        const gifResults1 = group!.shadowRoot!.querySelectorAll('emoji-image');
        assertEquals(gifResults1.length, 6);

        // Check display is correct.
        const leftColResults1 =
            group!.shadowRoot!.querySelectorAll<HTMLImageElement>(
                'div.left-column > emoji-image');
        assertEquals(leftColResults1.length, 3);
        assertEmojiImageAlt(leftColResults1[0], 'Trending Left 1');
        assertEmojiImageAlt(leftColResults1[1], 'Trending Left 2');
        assertEmojiImageAlt(leftColResults1[2], 'Trending Left 3');

        const rightColResults1 =
            group!.shadowRoot!.querySelectorAll<HTMLImageElement>(
                'div.right-column > emoji-image');
        assertEquals(rightColResults1.length, 3);
        assertEmojiImageAlt(rightColResults1[0], 'Trending Right 1');
        assertEmojiImageAlt(rightColResults1[1], 'Trending Right 2');
        assertEmojiImageAlt(rightColResults1[2], 'Trending Right 3');



        scrollToBottom();

        // Wait for Emoji Picker to scroll and render new GIFs.
        await waitForCondition(
            () => group!.shadowRoot!.querySelectorAll('emoji-image').length ===
                12,
            'failed to wait for new GIFs to render');

        const gifResults2 = group!.shadowRoot!.querySelectorAll('emoji-image');
        assertEquals(gifResults2.length, 12);

        // Check display is correct.
        const leftColResults2 =
            group!.shadowRoot!.querySelectorAll<HTMLImageElement>(
                'div.left-column > emoji-image');
        assertEquals(leftColResults2.length, 6);
        assertEmojiImageAlt(leftColResults2[0], 'Trending Left 1');
        assertEmojiImageAlt(leftColResults2[1], 'Trending Left 2');
        assertEmojiImageAlt(leftColResults2[2], 'Trending Left 3');
        assertEmojiImageAlt(leftColResults2[3], 'Trending Left 4');
        assertEmojiImageAlt(leftColResults2[4], 'Trending Left 5');
        assertEmojiImageAlt(leftColResults2[5], 'Trending Left 6');

        const rightColResults2 =
            group!.shadowRoot!.querySelectorAll<HTMLImageElement>(
                'div.right-column > emoji-image');
        assertEquals(rightColResults2.length, 6);
        assertEmojiImageAlt(rightColResults2[0], 'Trending Right 1');
        assertEmojiImageAlt(rightColResults2[1], 'Trending Right 2');
        assertEmojiImageAlt(rightColResults2[2], 'Trending Right 3');
        assertEmojiImageAlt(rightColResults2[3], 'Trending Right 4');
        assertEmojiImageAlt(rightColResults2[4], 'Trending Right 5');
        assertEmojiImageAlt(rightColResults2[5], 'Trending Right 6');
      });
});