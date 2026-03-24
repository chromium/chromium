// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://skills/skills_emoji_picker.js';

import type {SkillsEmojiPickerElement} from 'chrome://skills/skills_emoji_picker.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('SkillsEmojiPicker', function() {
  let emojiPicker: SkillsEmojiPickerElement;

  const TEST_EMOJI_DATA = [
    {
      group: 'Recent',
      emoji: [
        {
          base: [128054],  // 🐶
          shortcodes: [':dog:'],
        },
        {
          base: [128049],  // 🐱
          shortcodes: [':cat:'],
        },
      ],
    },
    {
      group: 'Smileys',
      emoji: [
        {
          base: [128512],  // 😀
          shortcodes: [':grinning:'],
        },
      ],
    },
  ];

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    emojiPicker = document.createElement('skills-emoji-picker');
    emojiPicker.setEmojiData(TEST_EMOJI_DATA);
    document.body.appendChild(emojiPicker);

    // Wait for the data to be "loaded" (it should be immediate).
    await microtasksFinished();
  });

  test('EmojiPickerLoadsData', function() {
    const categories =
        emojiPicker.shadowRoot.querySelectorAll('.category-title');
    assertEquals(2, categories.length);
    assertEquals('Recent', categories[0]!.textContent.trim());
    assertEquals('Smileys', categories[1]!.textContent.trim());

    const buttons = emojiPicker.shadowRoot.querySelectorAll('.emoji-button');
    assertEquals(3, buttons.length);
    assertEquals('🐶', buttons[0]!.textContent.trim());
    assertEquals('🐱', buttons[1]!.textContent.trim());
    assertEquals('😀', buttons[2]!.textContent.trim());
  });

  test('SearchFiltersEmojis', async function() {
    const searchInput = emojiPicker.$.searchInput;
    assertTrue(!!searchInput);

    // Filter for "dog"
    searchInput.value = 'dog';
    emojiPicker.setSearchDebounceDelayMsForTesting(0);
    searchInput.dispatchEvent(new CustomEvent('value-changed', {
      detail: {value: 'dog'},
    }));

    // Two microtasks are needed: one for the debounced search update to run,
    // and another for Lit to perform the subsequent batched DOM update.
    await microtasksFinished();
    await microtasksFinished();

    const buttons = emojiPicker.shadowRoot.querySelectorAll('.emoji-button');
    assertEquals(1, buttons.length);
    assertEquals('🐶', buttons[0]!.textContent.trim());

    const categories =
        emojiPicker.shadowRoot.querySelectorAll('.category-title');
    assertEquals(1, categories.length);
    assertEquals('Recent', categories[0]!.textContent.trim());
  });

  test('EmojiClickFiresEvent', async function() {
    const button =
        emojiPicker.shadowRoot.querySelector<HTMLElement>('.emoji-button');
    assertTrue(!!button);

    const eventPromise = eventToPromise('emoji-selected', emojiPicker);
    button.click();

    const event = await eventPromise;
    assertEquals('🐶', event.detail.emoji);
  });

  test('EscapeClosesPicker', async function() {
    const eventPromise = eventToPromise('picker-close', emojiPicker);

    emojiPicker.$.container.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'Escape',
      bubbles: true,
      composed: true,
    }));

    await eventPromise;
  });

  test('ClickOutsideClosesPicker', async function() {
    const eventPromise = eventToPromise('picker-close', emojiPicker);

    document.dispatchEvent(new MouseEvent('click', {
      bubbles: true,
      composed: true,
    }));

    await eventPromise;
  });

  test('KeyboardNavigation', async function() {
    const buttons =
        Array.from(emojiPicker.shadowRoot.querySelectorAll<HTMLButtonElement>(
            '.emoji-button'));
    assertTrue(buttons.length >= 2);

    // Initial focus - search input
    assertEquals(
        emojiPicker.$.searchInput, emojiPicker.shadowRoot.activeElement);

    // ArrowDown should focus first emoji if none focused
    emojiPicker.$.container.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'ArrowDown',
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();
    assertEquals(buttons[0], emojiPicker.shadowRoot.activeElement);

    // ArrowDown should focus next emoji
    buttons[0]!.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'ArrowDown',
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();
    assertEquals(buttons[1], emojiPicker.shadowRoot.activeElement);

    // ArrowUp should focus previous emoji
    buttons[1]!.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'ArrowUp',
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();
    assertEquals(buttons[0], emojiPicker.shadowRoot.activeElement);

    // ArrowRight should focus next emoji
    buttons[0]!.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'ArrowRight',
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();
    assertEquals(buttons[1], emojiPicker.shadowRoot.activeElement);

    // ArrowLeft should focus previous emoji
    buttons[1]!.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'ArrowLeft',
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();
    assertEquals(buttons[0], emojiPicker.shadowRoot.activeElement);
  });
});
