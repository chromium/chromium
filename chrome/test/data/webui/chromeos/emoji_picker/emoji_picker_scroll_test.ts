// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Category, EmojiPickerApiProxy} from 'chrome://emoji-picker/emoji_picker.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

import {initialiseEmojiPickerForTest} from './emoji_picker_test_util.js';

suite('<emoji-picker> scroll tests', () => {
  test('scrolls beyond 0 for symbols', async () => {
    // We need a promise to await to ensure that the scroll done event fires.
    // However, timing to create the promise is tricky since we need the emoji
    // picker to be created, and there may not be enough time inside the actual
    // test. On the other hand, when this api is called, we know that the emoji
    // picker exists, and we haven't begun the scroll so this timing should be
    // safe.
    let scrollDonePromise = new Promise(() => {});
    EmojiPickerApiProxy.getInstance().getInitialCategory = async () => {
      scrollDonePromise = new Promise((resolve) => {
        document.querySelector('emoji-picker-app')!.$.groups.onscrollend =
            resolve;
      });

      return {category: Category.kSymbols};
    };
    const {emojiPicker, readyPromise} = initialiseEmojiPickerForTest();
    await readyPromise;
    await scrollDonePromise;
    assertTrue(emojiPicker.$.groups.scrollTop > 0);
  });
  test('Does not scroll for emojis', async () => {
    EmojiPickerApiProxy.getInstance().getInitialCategory = async () => {
      return {category: Category.kEmojis};
    };
    const {emojiPicker, readyPromise} = initialiseEmojiPickerForTest();
    await readyPromise;
    // Calling scrollIntoView() for the first group is a no-op and doesn't fire
    // a scroll event. Instead just wait for a timeout to ensure that nothing
    // has happened. Not the most elegant solution, but should be good enough.
    await new Promise((resolve) => setTimeout(resolve, 2000));
    assertTrue(emojiPicker.$.groups.scrollTop === 0);
  });
});
