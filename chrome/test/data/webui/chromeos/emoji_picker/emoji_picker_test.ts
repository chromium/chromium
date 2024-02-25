// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EMOJI_PICKER_TOTAL_EMOJI_WIDTH, EMOJI_VARIANTS_SHOWN, EmojiButton, EmojiSearch} from 'chrome://emoji-picker/emoji_picker.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertFalse, assertGT, assertLT, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {assertCloseTo, dispatchMouseEvent, initialiseEmojiPickerForTest, isGroupButtonActive, timeout, waitForCondition, waitForEvent, waitWithTimeout} from './emoji_picker_test_util.js';


suite('<emoji-picker>', () => {
  const {emojiPicker, findInEmojiPicker, findEmojiFirstButton, readyPromise} =
      initialiseEmojiPickerForTest();

  setup(async () => {
    await readyPromise;
  });

  test('custom element should be defined', () => {
    assert(customElements.get('emoji-picker'));
  });

  test('first non-chevron, tab should be active by default', async () => {
    const button = findInEmojiPicker(
        'emoji-group-button[data-group="emoji-history"]', 'cr-icon-button');
    assertFalse(isGroupButtonActive(button));
  });

  test('second non-chevron tab should be inactive by default', () => {
    const button = findInEmojiPicker(
        'emoji-group-button[data-group="0"]', 'cr-icon-button');
    assertTrue(isGroupButtonActive(button));
  });

  test('Highlight bar should under emotions on start', () => {
    const button = findInEmojiPicker('#bar');
    assertCloseTo(
        EMOJI_PICKER_TOTAL_EMOJI_WIDTH, parseFloat(button!.style.left));
  });

  test('clicking second tab should activate it and scroll', async () => {
    // store initial scroll position of emoji groups.
    const emojiGroups = findInEmojiPicker('#groups')!;
    const initialScroll = emojiGroups.scrollTop;

    // History group doesn't exist when there is no history, so scrolling to
    // the first non-history group (0) may not trigger a scroll, so scroll to
    // group (1).
    const firstButton = findInEmojiPicker(
        'emoji-group-button[data-group="emoji-history"]', 'cr-icon-button');
    const thirdButton = findInEmojiPicker(
        'emoji-group-button[data-group="1"]', 'cr-icon-button');

    await waitForCondition(
        () => findEmojiFirstButton('[data-group="2"] > emoji-group'),
        'wait for groups to render so they can be scrolled to');
    thirdButton!.click();

    await waitForCondition(
        () => isGroupButtonActive(thirdButton) &&
            !isGroupButtonActive(firstButton),
        'wait for scroll to happen and buttons to update');

    const newScroll = emojiGroups.scrollTop;
    assertGT(newScroll, initialScroll);

    // check the highlight bar also moved
    const bar = findInEmojiPicker('#bar');
    assertGT(parseInt(bar!.style.left, 10), 0);
  });

  test('recently used should be hidden when empty', () => {
    const recentlyUsed =
        findInEmojiPicker('[data-group="emoji-history"] > emoji-group');
    assert(recentlyUsed!.classList.contains('hidden'));
  });

  test(
      'recently used should be populated after emoji is clicked normally',
      async () => {
        emojiPicker.updateIncognitoState(false);
        const emojiButton = await waitForCondition(
            () => findEmojiFirstButton('[data-group="0"] > emoji-group'),
            'wait for emoji-group to render');
        emojiButton!.click();

        const recentlyUsed = await waitForCondition(
            () => findEmojiFirstButton(
                '[data-group="emoji-history"] > emoji-group'),
            'wait for recently used to exists');

        // check text is correct.
        const recentText = recentlyUsed!.innerText;
        assertTrue(recentText.includes(String.fromCodePoint(128512)));
      });

  test(
      'clicking an emoji with no text field should copy it to the clipboard',
      async () => {
        emojiPicker.updateIncognitoState(false);
        const emojiButton = await waitForCondition(
            () => findEmojiFirstButton('[data-group="0"] > emoji-group'),
            'wait for emoji group to render');
        emojiButton!.click();

        await waitForCondition(
            () => findEmojiFirstButton(
                '[data-group="emoji-history"] > emoji-group'),
            'wait until recently used exists');

        await (waitForCondition(async () => {
          const clipboardtext = await navigator.clipboard.readText();
          return clipboardtext === String.fromCodePoint(128512);
        }, 'ensure correct text exists'));
      });

  test('recently-used should have variants for variant emoji', async () => {
    emojiPicker.updateIncognitoState(false);
    const emojiButton = (await waitForCondition(
        () => findInEmojiPicker(
            '[data-group="0"] > emoji-group', 'button[data-index="2"]'),
        'wait for emoji group to render'));
    emojiButton!.click();

    // wait until emoji exists in recently used section.
    const recentlyUsed = (await waitForCondition(
        () =>
            findEmojiFirstButton('[data-group="emoji-history"] > emoji-group'),
        'wait for recently used to render'));

    // check variants class is applied
    assertTrue(recentlyUsed!.classList.contains('has-variants'));
  });

  test(
      'recently-used should have no variants for non-variant emoji',
      async () => {
        emojiPicker.updateIncognitoState(false);
        const emojiButton = (await waitForCondition(
            () => findInEmojiPicker(
                '[data-group="0"] > emoji-group', 'button[data-index="0"]'),
            'ensure all emoji rendered'));
        emojiButton!.click();

        const recentlyUsed = (await waitForCondition(
            () => findEmojiFirstButton(
                '[data-group="emoji-history"] > emoji-group'),
            'wait for recently used to contain correct emoji'));

        // check variants class is not applied
        assertFalse(recentlyUsed!.classList.contains('has-variants'));
      });

  test(
      'recently-used should be empty after emoji is clicked in incognito mode',
      async () => {
        emojiPicker.updateIncognitoState(true);
        const emojiButton = await waitForCondition(
            () => findEmojiFirstButton('[data-group="0"] > emoji-group'),
            'ensure emoji picker fully rendered');
        emojiButton!.click();

        // Wait to ensure recents has a chance to render if we have a bug.
        await timeout(1000);

        const recentlyUsed =
            findInEmojiPicker('[data-group="emoji-history"] > emoji-group');
        assert(recentlyUsed!.classList.contains('hidden'));
      });

  test('recently used should be empty after clearing', async () => {
    emojiPicker.updateIncognitoState(false);
    const emojiButton = (await waitForCondition(
        () => findInEmojiPicker(
            '[data-group="0"] > emoji-group', 'button[data-index="1"]'),
        'ensure all emoji rendered'));
    emojiButton!.click();

    (await waitForCondition(
        () =>
            findEmojiFirstButton('[data-group="emoji-history"] > emoji-group'),
        'wait until emoji in recently used'));

    // click show clear button
    findInEmojiPicker('.group', '#show-clear')!.click();
    await waitForCondition(
        () => findInEmojiPicker('.group', '#clear-recents'),
        'wait for clear recents to appear');

    // click clear button
    findInEmojiPicker('.group', '#clear-recents')!.click();

    // Expect no more history.
    await waitForCondition(
        () => findInEmojiPicker('[data-group="emoji-history"] > emoji-group')!
                  .classList.contains('hidden'),
        'history failed to disappear');
  });


  suite('<emoji-variants>', () => {
    let firstEmojiButton: EmojiButton;

    const findEmojiVariants = (el: EmojiButton) => {
      const variants = (el.querySelector('emoji-variants'));
      return variants && variants.style.display !== 'none' ? variants : null;
    };

    setup(async () => {
      firstEmojiButton = (await waitForCondition(
                             () => findInEmojiPicker(
                                 '[data-group="0"] > emoji-group',
                                 '.emoji-button-container:nth-child(3)'),
                             'ensure all emoji are rendered')) as EmojiButton;

      // right click and wait for variants to appear.
      const variantsPromise = waitForEvent(emojiPicker, EMOJI_VARIANTS_SHOWN);
      dispatchMouseEvent(firstEmojiButton.querySelector('button')!, 2);

      await waitWithTimeout(
          variantsPromise, 1000, 'did not receive emoji variants event.');
      await waitForCondition(
          () => findEmojiVariants(firstEmojiButton),
          'emoji-variants failed to appear.', 5000);
    });

    test('right clicking emoji again should close popup', async () => {
      // right click again and variants should disappear.
      dispatchMouseEvent(firstEmojiButton.querySelector('button')!, 2);
      await waitForCondition(
          () => !findEmojiVariants(firstEmojiButton),
          'emoji-variants failed to disappear.');
    });

    test('clicking elsewhere should close popup', async () => {
      // click in some empty space of main emoji picker.
      emojiPicker.click();

      await waitForCondition(
          () => !findEmojiVariants(firstEmojiButton),
          'emoji-variants failed to disappear.');
    });

    test('opening different variants should close first variants', async () => {
      const emojiButton2 = await waitForCondition(
                               () => findInEmojiPicker(
                                   '[data-group="0"] > emoji-group',
                                   '.emoji-button-container:nth-child(4)'),
                               'ensure all emoji are rendered') as EmojiButton;

      // right click on second emoji button
      dispatchMouseEvent(emojiButton2.querySelector('button')!, 2);
      // ensure first popup disappears and second popup appears.
      await waitForCondition(
          () => !findEmojiVariants(firstEmojiButton),
          'first emoji-variants failed to disappear.');
      await waitForCondition(
          () => findEmojiVariants(emojiButton2),
          'second emoji-variants failed to appear.');
    });

    test('opening variants on the left side should not overflow', async () => {
      const groupsRect = emojiPicker.getBoundingClientRect();

      const variants = findEmojiVariants(firstEmojiButton);
      const variantsRect = variants!.getBoundingClientRect();

      assertLT(groupsRect.left, variantsRect.left);
      assertLT(variantsRect.right, groupsRect.right);
    });

    test('opening large variants (twice) should not overflow', async () => {
      const coupleEmojiButton =
          (await waitForCondition(
              () => findInEmojiPicker(
                  '[data-group="0"] > emoji-group',
                  '.emoji-button-container:nth-child(5)'),
              'ensure all emoji are rendered')) as EmojiButton;

      // listen for emoji variants event.
      const variantsPromise = waitForEvent(emojiPicker, EMOJI_VARIANTS_SHOWN);

      // right click on couple emoji to show 5x5 grid with skin tone.
      dispatchMouseEvent(coupleEmojiButton.querySelector('button')!, 2);
      const variants = await waitForCondition(
          () => findEmojiVariants(coupleEmojiButton),
          'Ensure correct emoji is rendered');

      // ensure variants are positioned before we get bounding rectangle.
      await waitWithTimeout(
          variantsPromise, 1000, 'did not receive emoji variants event.');
      const variantsRect = variants!.getBoundingClientRect();
      const pickerRect = emojiPicker.getBoundingClientRect();

      assertLT(pickerRect.left, variantsRect.left);
      assertLT(variantsRect.right, pickerRect.right);

      // Now close the variants and reopen, should still be positioned properly.
      emojiPicker.click();
      await waitForCondition(
          () => !findEmojiVariants(firstEmojiButton),
          'emoji-variants failed to disappear.');

      const variantsPromise2 = waitForEvent(emojiPicker, EMOJI_VARIANTS_SHOWN);
      // reshow
      dispatchMouseEvent(coupleEmojiButton.querySelector('button')!, 2);
      const variants2 = await waitForCondition(
          () => findEmojiVariants(coupleEmojiButton),
          'ensure that correct emoji is rendered')!;
      // ensure variants are positioned before we get bounding rectangle.
      await waitWithTimeout(
          variantsPromise2, 1000,
          'did not receive second emoji variants event.');
      const variantsRect2 = variants2!.getBoundingClientRect();

      assertLT(pickerRect.left, variantsRect2.left);
      assertLT(variantsRect2.right, pickerRect.right);
    });
  });

  suite('<emoji-search>', () => {
    test('works when there are no results', () => {
      // This test just ensures that no errors are thrown.
      const enterEvent = new KeyboardEvent(
          'keydown', {cancelable: true, key: 'Enter', keyCode: 13});
      const search = findInEmojiPicker('emoji-search') as EmojiSearch;
      search.onSearchKeyDown(enterEvent);
    });
    test('finds results in the second group', async () => {
      const search = findInEmojiPicker('emoji-search') as EmojiSearch;
      // This particular emoji only appears in the third tab of the test
      // ordering
      search.setSearchQuery('face with tears of joy');

      await waitForCondition(
          () => search.getNumSearchResults() > 0,
          'wait for search results to exist');
    });
    test('finds no results for garbage search', async () => {
      const search = findInEmojiPicker('emoji-search') as EmojiSearch;
      search!.setSearchQuery('THIS string should not match anything');

      await waitForCondition(
          () => findInEmojiPicker('emoji-search', '.no-result'),
          'wait for no results');
      assertEquals(search!.getNumSearchResults(), 0);
    });
  });
});
