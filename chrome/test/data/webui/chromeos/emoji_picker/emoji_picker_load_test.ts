// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EmojiPickerApiProxy, EmojiSearch} from 'chrome://emoji-picker/emoji_picker.js';
import {assertEquals, assertGT} from 'chrome://webui-test/chai_assert.js';

import {initialiseEmojiPickerForTest, waitForCondition} from './emoji_picker_test_util.js';
import {TestEmojiPickerApiProxy} from './test_emoji_picker_api_proxy.js';

suite('emoji-picker-load', () => {
  const testApiProxy = new TestEmojiPickerApiProxy();
  testApiProxy.getInitialQuery = async () => {
    return {query: 'a'};
  };
  EmojiPickerApiProxy.setInstance(testApiProxy);

  const {findInEmojiPicker, readyPromise} = initialiseEmojiPickerForTest();
  let emojiSearch: EmojiSearch;

  setup(async () => {
    await readyPromise;
    emojiSearch = findInEmojiPicker('emoji-search') as EmojiSearch;
  });

  test('Prefills emoji search when given an initial query.', async () => {
    await waitForCondition(
        () =>
            findInEmojiPicker('emoji-search', 'emoji-group[category="emoji"]'),
        'wait for search results to enter');
    const emojiResults =
        findInEmojiPicker(
            'emoji-search',
            'emoji-group')!.shadowRoot!.querySelectorAll('.emoji-button');
    assertGT(emojiResults.length, 0);
    assertEquals(emojiSearch.getSearchQuery(), 'a');
  });
});
