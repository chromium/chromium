// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GIF_VALIDATION_DATE} from 'chrome://emoji-picker/constants.js';
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

suite(`emoji-picker-validation-gif`, () => {
  /** @type {!EmojiPicker} */
  let emojiPicker;
  /** @type {function(...!string): ?HTMLElement} */
  let findInEmojiPicker;
  /** @type {function(...!string): ?HTMLElement} */
  let findEmojiAllButtons;
  /** @type {Array<string>} */
  const categoryList = ['emoji', 'symbol', 'emoticon', 'gif'];
  /** @type {number} */
  let categoryIndex;

  setup(() => {
    // Reset DOM state.
    document.body.innerHTML = '';
    window.localStorage.clear();
    window.localStorage.setItem(GIF_VALIDATION_DATE, new Date(0).toJSON());

    const historyGifs = {
      history: [
        {
          base: {
            visualContent: {
              id: '0',
              url: {
                full: {
                  url:
                      'https://media.tenor.com/P0tX6a_nVIkAAAAC/grinch-smile-grinch.gif',
                },
                preview: {
                  url:
                      'https://media.tenor.com/P0tX6a_nVIkAAAAM/grinch-smile-grinch.gif',
                },
              },
              previewSize: {width: 220, height: 324},
            },
            name: 'Grinch Smile Grinch GIF',
          },
          alternates: [],
        },
        {
          base: {
            visualContent: {
              id: '1',
              url: {
                full: {
                  url: 'https://media.tenor.com/HU6E9HN1dSAAAAAC/head-turn.gif',
                },
                preview: {
                  url: 'https://media.tenor.com/HU6E9HN1dSAAAAAM/head-turn.gif',
                },
              },
              previewSize: {width: 220, height: 224},
            },
            name: 'Head Turn GIF',
          },
          alternates: [],
        },
        {
          base: {
            visualContent: {
              id: '2',
              url: {
                full: {
                  url:
                      'https://media.tenor.com/OfjkK_lANHsAAAAC/snoopy-dog.gif',
                },
                preview: {
                  url:
                      'https://media.tenor.com/OfjkK_lANHsAAAAM/snoopy-dog.gif',
                },
              },
              previewSize: {width: 220, height: 164},
            },
            name: 'Snoopy Dog GIF',
          },
          alternates: [],
        },
        {
          base: {
            visualContent: {
              id: 3,
              url: {
                full: {
                  url:
                      'https://media.tenor.com/lSFT81zyIkMAAAAC/baby-yoda-i-love-you.gif',
                },
                preview: {
                  url:
                      'https://media.tenor.com/lSFT81zyIkMAAAAM/baby-yoda-i-love-you.gif',
                },
              },
              previewSize: {width: 220, height: 176},
            },
            name: 'Baby Yoda I Love You GIF',
          },
          alternates: [],
        },
        {
          base: {
            visualContent: {
              id: '4',
              url: {
                full: {
                  url:
                      'https://media.tenor.com/QPapotlLW18AAAAC/dance-cute.gif',
                },
                preview: {
                  url:
                      'https://media.tenor.com/QPapotlLW18AAAAM/dance-cute.gif',
                },
              },
              previewSize: {width: 220, height: 234},
            },
            name: 'Dance Cute GIF',
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

        // Check that the correct GIFs have been deleted.
        assert(recentlyUsedEmoji[0].alt === 'Head Turn GIF');
        assert(recentlyUsedEmoji[1].alt === 'Snoopy Dog GIF');
        assert(recentlyUsedEmoji[2].alt === 'Dance Cute GIF');
      });
});