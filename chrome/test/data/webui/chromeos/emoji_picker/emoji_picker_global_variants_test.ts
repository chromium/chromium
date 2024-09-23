// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Definition of emoji skin tone and gender categorizations:
 *  - Group 1: emojis with only skin tone, e.g. 👍
 *  - Group 2: emojis with only gender, e.g. 🧞
 *  - Group 3: emojis with both tone and gender, e.g. 🤷
 *  - Group 4: the multi-part emoji with only tone: 🤝
 *  - Group 5: multi-part emojis with tone and gender, e.g. 👬
 */

import {initialiseEmojiPickerForTest} from './emoji_picker_test_util.js';

suite('emoji-picker-global-variants', () => {
  let expectEmojiButton: (text: string, getGroup?: () => HTMLElement | null) =>
      Promise<HTMLElement>;
  let expectEmojiButtons:
      (texts: string[], getGroup?: () => HTMLElement | null) =>
          Promise<HTMLElement[]>;
  let clickVariant: (text: string, button: HTMLElement) => Promise<void>;
  let findGroup: (groupId: string) => HTMLElement | null;
  let reload: () => Promise<void>;

  setup(async () => {
    const newPicker = initialiseEmojiPickerForTest();
    expectEmojiButton = newPicker.expectEmojiButton;
    expectEmojiButtons = newPicker.expectEmojiButtons;
    clickVariant = newPicker.clickVariant;
    findGroup = newPicker.findGroup;
    reload = newPicker.reload;
    await newPicker.readyPromise;
  });

  test('tone should sync from group 1 to group 2', async () => {
    const thumbsUp = await expectEmojiButton('👍');

    await clickVariant('👍🏿', thumbsUp);
    await reload();

    await expectEmojiButtons(['👍🏿', '🤷🏿']);
  });

  test('gender should sync from group 3 to group 2', async () => {
    const genie = await expectEmojiButton('🧞');

    await clickVariant('🧞‍♀', genie);
    await reload();

    await expectEmojiButtons(['🧞‍♀', '🤷‍♀']);
  });

  test('tone & gender should sync from groups 1 & 3 to group 2', async () => {
    const thumbsUp = await expectEmojiButton('👍');

    const genie = await expectEmojiButton('🧞');
    await clickVariant('👍🏿', thumbsUp);
    await clickVariant('🧞‍♀', genie);
    await reload();

    await expectEmojiButton('🤷🏿‍♀');
  });

  test('tone & gender should sync from groups 2 to group 1 & 3', async () => {
    const shrug = await expectEmojiButton('🤷');

    await clickVariant('🤷🏿‍♀', shrug);
    await reload();

    await expectEmojiButtons(['🤷🏿‍♀', '👍🏿', '🧞‍♀']);
  });

  test('tone preference should be individual for group 4', async () => {
    const handshake = await expectEmojiButton('🤝');

    await clickVariant('🫱🏻‍🫲🏿', handshake);
    await reload();

    await expectEmojiButtons(['🫱🏻‍🫲🏿', '👬', '👍', '🤷']);
  });

  test('tone preference should be individual for group 5', async () => {
    const holdingHands = await expectEmojiButton('👬');

    await clickVariant('👨🏿‍🤝‍👨🏻', holdingHands);
    await reload();

    await expectEmojiButtons(['👨🏿‍🤝‍👨🏻', '🤝', '👍', '🤷']);
  });

  test(
      'tone preference for multi-part emojis with a single tone codepoint should be individual',
      async () => {
        const handshake = await expectEmojiButton('🤝');

        await clickVariant('🤝🏿', handshake);
        await reload();

        await expectEmojiButtons(['🤝🏿', '👍']);
      });

  test(
      'selecting a variant with default tone should update preferences',
      async () => {
        const shrug = await expectEmojiButton('🤷');
        const thumbsUp = await expectEmojiButton('👍');

        await clickVariant('🤷🏿‍♀', shrug);
        await clickVariant('👍', thumbsUp);
        await reload();

        await expectEmojiButtons(['🤷‍♀', '👍']);
      });

  test(
      'selecting a variant with default gender should update preferences',
      async () => {
        const shrug = await expectEmojiButton('🤷');
        const genie = await expectEmojiButton('🧞');

        await clickVariant('🤷🏿‍♀', shrug);
        await clickVariant('🧞', genie);
        await reload();

        await expectEmojiButtons(['🤷🏿', '🧞']);
      });

  test(
      'selecting a variant from history should update preferences',
      async () => {
        const shrug = await expectEmojiButton('🤷');
        shrug.click();
        const historyEmoji =
            await expectEmojiButton('🤷', () => findGroup('emoji-history'));

        await clickVariant('🤷🏿‍♀', historyEmoji);
        await reload();

        await expectEmojiButtons(['🤷🏿‍♀', '👍🏿', '🧞‍♀']);
      });

  test(
      'selecting an emoji from history without opening variants should not update preferences',
      async () => {
        const shrug = await expectEmojiButton('🤷');
        await clickVariant('🤷🏿‍♀', shrug);
        await clickVariant('🤷🏻‍♂', shrug);

        const historyEmoji = await expectEmojiButton(
            '🤷🏿‍♀', () => findGroup('emoji-history'));
        historyEmoji.click();
        await reload();

        await expectEmojiButtons(['🤷🏻‍♂', '👍🏻', '🧞‍♂']);
      });

  test('preferences should not be applied in emoji history', async () => {
    const shrug = await expectEmojiButton('🤷');

    await clickVariant('🤷🏻‍♂', shrug);
    await clickVariant('🤷🏿‍♀', shrug);

    await expectEmojiButtons(
        ['🤷🏿‍♀', '🤷🏻‍♂'], () => findGroup('emoji-history'));
  });

  test('preferences should be applied in other subcategories', async () => {
    const shrug = await expectEmojiButton('🤷');

    await clickVariant('🤷🏿‍♀', shrug);
    await reload();

    await expectEmojiButton('🧍🏿‍♀', () => findGroup('1'));
  });
});
