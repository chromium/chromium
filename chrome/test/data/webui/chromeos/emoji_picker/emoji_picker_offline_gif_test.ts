// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EmojiPickerApiProxy, EmojiSearch, TRENDING_GROUP_ID} from 'chrome://emoji-picker/emoji_picker.js';
import {assert} from 'chrome://resources/js/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {initialiseEmojiPickerForTest, waitForCondition} from './emoji_picker_test_util.js';
import {TestEmojiPickerApiProxyError} from './test_emoji_picker_offline_api_proxy.js';

function subcategoryGroupSelector(category: string, subcategory: string) {
  return `[data-group="${subcategory}"] > ` +
      `emoji-group[category="${category}"]`;
}

suite('emoji-picker-offline-gif', () => {
  EmojiPickerApiProxy.setInstance(new TestEmojiPickerApiProxyError());
  (EmojiPickerApiProxy.getInstance() as TestEmojiPickerApiProxyError)
      .setNetError();
  const {emojiPicker, findInEmojiPicker, readyPromise} =
      initialiseEmojiPickerForTest();
  let emojiSearch: EmojiSearch;
  const categoryList = ['emoji', 'symbol', 'emoticon', 'gif'];
  let categoryIndex: number;

  setup(async () => {
    categoryIndex = categoryList.indexOf('gif');

    await readyPromise;
    emojiSearch = findInEmojiPicker('emoji-search') as EmojiSearch;
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
      'There exists emoji-error component in the Emoji Category.',
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

        const errorText = errorElement!.shadowRoot!.querySelector(
            '.gif-error-container > .error-text');
        assertEquals(
            errorText!.textContent,
            'Connect to the internet to search for GIFs');
      });
});
