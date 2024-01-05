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

import {EmojiButton, EmojiSearch} from 'chrome://emoji-picker/emoji_picker.js';

import {dispatchMouseEvent, initialiseEmojiPickerForTest, waitForCondition} from './emoji_picker_test_util.js';

suite('emoji-picker-global-variants', () => {
  let findInEmojiPicker: (...path: string[]) => HTMLElement | null;
  let findEmojiButtonByText: (text: string, group: HTMLElement) =>
      HTMLElement | null;
  let findGroup: (groupId: string) => HTMLElement | null;
  let findSearchGroup: (category: string) => HTMLElement | null;
  let reload: () => Promise<void>;
  let setIncognito: (incognito: boolean) => void;

  const expectEmojiButton = (text: string, getGroup = () => findGroup('0')) =>
      waitForCondition(() => {
        const group = getGroup();
        return group ? findEmojiButtonByText(text, group) : null;
      }, `wait for emoji ${text} to render`);

  const expectEmojiButtons =
      (texts: string[], getGroup?: () => HTMLElement | null) =>
          Promise.all(texts.map(text => expectEmojiButton(text, getGroup)));

  const findEmojiVariant = (text: string, button: HTMLElement) => {
    const variants =
        button.parentElement?.querySelector<HTMLElement>('emoji-variants');

    if (!variants || variants.style.display === 'none') {
      return null;
    }

    const variantButtons =
        Array.from(variants?.shadowRoot!.querySelectorAll('emoji-button'));
    const component =
        variantButtons.find(button => (button as EmojiButton).emoji === text);

    return component?.shadowRoot!.querySelector<HTMLElement>('#emoji-button') ??
        null;
  };

  const setSearchQuery = (value: string) => {
    const emojiSearch = findInEmojiPicker('emoji-search') as EmojiSearch;
    emojiSearch.setSearchQuery(value);
  };

  const clickVariant = async (text: string, button: HTMLElement) => {
    dispatchMouseEvent(button, 2);
    const variant = await waitForCondition(
        () => findEmojiVariant(text, button),
        `wait for variants for emoji ${text} to render`);
    variant.click();
  };

  setup(async () => {
    const newPicker = initialiseEmojiPickerForTest();
    findInEmojiPicker = newPicker.findInEmojiPicker;
    findEmojiButtonByText = newPicker.findEmojiButtonByText;
    findGroup = newPicker.findGroup;
    findSearchGroup = newPicker.findSearchGroup;
    reload = newPicker.reload;
    setIncognito = newPicker.setIncognito;
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
      'selecting a variant from search should update preferences', async () => {
        setSearchQuery('shrug');
        const searchEmoji =
            await expectEmojiButton('🤷', () => findSearchGroup('emoji'));
        await clickVariant('🤷🏿‍♀', searchEmoji);
        await reload();
        await expectEmojiButtons(['🤷🏿‍♀', '👍🏿', '🧞‍♀']);
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
      'selecting a variant from existing history should migrate it and update preferences',
      async () => {
        window.localStorage.setItem('emoji-recently-used', JSON.stringify({
          history: [{
            base: {string: '🧞‍♀', name: ' genie'},
            alternates: [
              {string: '🧞', name: 'genie'},
              {string: '🧞‍♀', name: 'woman genie'},
              {string: '🧞‍♂', name: 'man genie'},
            ],
          }],
        }));
        await reload();
        const historyEmoji = await expectEmojiButton(
            '🧞‍♀', () => findGroup('emoji-history'));
        await clickVariant('🧞‍♂', historyEmoji);
        await reload();
        await expectEmojiButtons(['🤷‍♂', '🧞‍♂']);
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

  test('preferences should be applied in emoji search', async () => {
    const thumbsUp = await expectEmojiButton('👍');
    await clickVariant('👍🏿', thumbsUp);
    await reload();
    setSearchQuery('shrug');
    await expectEmojiButton('🤷🏿', () => findSearchGroup('emoji'));
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

  test(
      'existing preferences should remain until setting a global preference',
      async () => {
        window.localStorage.setItem(
            'emoji-recently-used',
            JSON.stringify(
                {history: [], preference: {'🤷': '🤷🏿‍♀'}}));
        await reload();
        await expectEmojiButton('🤷🏿‍♀');
        const thumbsUp = await expectEmojiButton('👍');
        await clickVariant('👍🏻', thumbsUp);
        await reload();
        await expectEmojiButton('🤷🏻‍♀');
        const genie = await expectEmojiButton('🧞');
        await clickVariant('🧞‍♂', genie);
        await reload();
        await expectEmojiButton('🤷🏻‍♂');
      });

  test(
      'selecting a base emoji should not overwrite individual preferences with the default',
      async () => {
        window.localStorage.setItem(
            'emoji-recently-used',
            JSON.stringify(
                {history: [], preference: {'🤷': '🤷🏿‍♀'}}));
        await reload();
        const thumbsUp = await expectEmojiButton('👍');
        thumbsUp.click();
        await reload();
        await expectEmojiButton('🤷🏿‍♀');
      });

  test(
      'preferences should not be saved when selecting a variant in incognito',
      async () => {
        setIncognito(true);
        await reload();
        const shrug = await expectEmojiButton('🤷');
        await clickVariant('🤷🏿‍♀', shrug);
        await reload();
        await expectEmojiButtons(['🤷', '👍', '🧞']);
      });

  test('existing preferences should not be applied in incognito', async () => {
    const shrug = await expectEmojiButton('🤷');
    await clickVariant('🤷🏿‍♀', shrug);
    setIncognito(true);
    await reload();
    await expectEmojiButtons(['🤷', '👍', '🧞']);
  });
});
