// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TRENDING_GROUP_ID} from 'chrome://emoji-picker/constants.js';
import {EmojiPicker} from 'chrome://emoji-picker/emoji_picker.js';
import {EmojiPickerApiProxyImpl} from 'chrome://emoji-picker/emoji_picker_api_proxy.js';
import {EmojiSearch} from 'chrome://emoji-picker/emoji_search.js';
import {EMOJI_PICKER_READY} from 'chrome://emoji-picker/events.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {deepQuerySelector, waitForCondition} from './emoji_picker_test_util.js';
import {TestEmojiPickerApiProxyErrorImpl} from './test_emoji_picker_offline_api_proxy.js';

function subcategoryGroupSelector(category: string, subcategory: string) {
  return `[data-group="${subcategory}"] > ` +
      `emoji-group[category="${category}"]`;
}

suite('emoji-picker-offline-gif', () => {
  let emojiPicker: EmojiPicker;
  let findInEmojiPicker: (...selectors: string[]) => HTMLElement | null;
  let emojiSearch: EmojiSearch;
  const categoryList = ['emoji', 'symbol', 'emoticon', 'gif'];
  let categoryIndex: number;

  setup(() => {
    // Reset DOM state.
    document.body.innerHTML = '';
    window.localStorage.clear();

    EmojiPickerApiProxyImpl.setInstance(new TestEmojiPickerApiProxyErrorImpl());

    // Set default incognito state to False.
    EmojiPickerApiProxyImpl.getInstance().isIncognitoTextField = () =>
        new Promise((resolve) => resolve({incognito: false}));

    (EmojiPickerApiProxyImpl.getInstance() as TestEmojiPickerApiProxyErrorImpl)
        .setNetError();

    EmojiPicker.configs = () => ({
      dataUrls: {
        emoji: [
          '/emoji_test_ordering_start.json',
          '/emoji_test_ordering_remaining.json',
        ],
        emoticon: ['/emoticon_test_ordering.json'],
        symbol: ['/symbol_test_ordering.json'],
        gif: [],
      },
    });

    emojiPicker =
        (document.createElement('emoji-picker')) as unknown as EmojiPicker;

    findInEmojiPicker = (...path) => deepQuerySelector(emojiPicker, path);

    categoryIndex = categoryList.indexOf('gif');

    // Wait until emoji data is loaded before executing tests.
    return new Promise<void>((resolve) => {
      emojiPicker.addEventListener(EMOJI_PICKER_READY, () => {
        flush();
        resolve();
      });
      document.body.appendChild(emojiPicker);
      emojiSearch = findInEmojiPicker('emoji-search') as unknown as EmojiSearch;
    });
  });

  test('There is no trending GIFs.', async () => {
    const categoryButton =
        emojiSearch.shadowRoot!
            .querySelectorAll('emoji-category-button')[categoryIndex]!
            .shadowRoot!.querySelector('cr-icon-button');
    categoryButton!.click();
    flush();

    await waitForCondition(
        () => emojiPicker.activeInfiniteGroupId === TRENDING_GROUP_ID,
        'wait for correct groupID');

    const gifResults = findInEmojiPicker(
        subcategoryGroupSelector('gif', emojiPicker.activeInfiniteGroupId!));
    assert(!gifResults);
  });

  test(
      'There exists emoji-error component in the Trending category.',
      async () => {
        const categoryButton =
            emojiSearch.shadowRoot!
                .querySelectorAll('emoji-category-button')[categoryIndex]!
                .shadowRoot!.querySelector('cr-icon-button');
        categoryButton!.click();
        flush();

        const errorElement =
            findInEmojiPicker('#list-container', '#groups', 'emoji-error')!;
        assert(errorElement);

        assert(errorElement.shadowRoot!.querySelector(
            '.gif-error-container > #no-internet-icon'));
        const errorText = errorElement.shadowRoot!.querySelector(
            '.gif-error-container > .error-text');
        assertEquals(
            errorText!.textContent, 'Connect to the internet to view GIFs');
      });

  test(
      `There exists emoji-error component when searching for something that is
      not in emoji, symbol or emoticon.`,
      async () => {
        emojiSearch.setSearchQuery('abc');
        const results = await waitForCondition(
            () => emojiSearch.shadowRoot!.getElementById('results'),
            'wait for results');
        const errorElement = results!.querySelector('.no-result > emoji-error');
        assert(errorElement);

        assert(errorElement!.shadowRoot!.querySelector(
            '.gif-error-container > #no-internet-icon'));
        const errorText = errorElement!.shadowRoot!.querySelector(
            '.gif-error-container > .error-text');
        assertEquals(
            errorText!.textContent,
            'Connect to the internet to search for GIFs');
      });
});
