// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EmojiPickerApiProxy, EmojiSearch, TRENDING_GROUP_ID} from 'chrome://emoji-picker/emoji_picker.js';
import {assert} from 'chrome://resources/js/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {initialiseEmojiPickerForTest, waitForCondition} from './emoji_picker_test_util.js';
import {TestEmojiPickerApiProxyError} from './test_emoji_picker_offline_api_proxy.js';

const CATEGORY_LIST = ['emoji', 'symbol', 'emoticon', 'gif'];
function subcategoryGroupSelector(category: string, subcategory: string) {
  return `[data-group="${subcategory}"] > ` +
      `emoji-group[category="${category}"]`;
}

suite('emoji-picker-offline-gif', () => {
  const testEmojiPickerApiProxy = new TestEmojiPickerApiProxyError();
  EmojiPickerApiProxy.setInstance(testEmojiPickerApiProxy);
  testEmojiPickerApiProxy.setHttpError();
  const {emojiPicker, findInEmojiPicker, readyPromise} =
      initialiseEmojiPickerForTest();
  let emojiSearch: EmojiSearch;
  let categoryIndex: number;

  setup(async () => {
    await readyPromise;
    emojiSearch = findInEmojiPicker('emoji-search') as EmojiSearch;
    categoryIndex = CATEGORY_LIST.indexOf('gif');
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
        'Wait for correct activeInfiniteGroupId to be set.');

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
            findInEmojiPicker('#list-container', '#groups', 'emoji-error');
        assert(errorElement);

        const genericErrorImageNew =
            errorElement!.shadowRoot!.querySelector('.gif-error-container svg');
        assert(genericErrorImageNew);

        const errorText = errorElement!.shadowRoot!.querySelector(
            '.gif-error-container > .error-text');
        assertEquals(errorText!.textContent, 'Something went wrong');
      });

  test(
      `There exists emoji-error component when searching for something that is
      not in emoji, symbol or emoticon.`,
      async () => {
        emojiSearch.setSearchQuery('abc');
        const results = await waitForCondition(
            () => emojiSearch.shadowRoot!.getElementById('results'),
            'wait for search results');
        const errorElement = results!.querySelector('.no-result > emoji-error');
        assert(errorElement);

        const genericErrorImageNew =
            errorElement!.shadowRoot!.querySelector('.gif-error-container svg');
        assert(genericErrorImageNew);

        const errorText = errorElement!.shadowRoot!.querySelector(
            '.gif-error-container > .error-text');
        assertEquals(errorText!.textContent, 'Something went wrong');
      });
});
