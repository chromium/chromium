// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_profile_avatar_selector/cr_profile_avatar_selector.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrProfileAvatarSelectorElement} from 'chrome://resources/cr_elements/cr_profile_avatar_selector/cr_profile_avatar_selector.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {keyDownOn, pressAndReleaseKeyOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';
// clang-format on

/** @fileoverview Suite of tests for cr-profile-avatar-selector. */
suite('cr-profile-avatar-selector', function() {
  let avatarSelector: CrProfileAvatarSelectorElement;

  function createElement(): CrProfileAvatarSelectorElement {
    const avatarSelector = document.createElement('cr-profile-avatar-selector');
    avatarSelector.avatars = [
      {
        url: 'chrome://avatar1.png',
        label: 'avatar1',
        index: 1,
        selected: false,
        isGaiaAvatar: false,
      },
      {
        url: 'chrome://avatar2.png',
        label: 'avatar2',
        index: 2,
        selected: false,
        isGaiaAvatar: false,
      },
      {
        url: 'chrome://avatar3.png',
        label: 'avatar3',
        index: 3,
        selected: false,
        isGaiaAvatar: false,
      },
    ];
    return avatarSelector;
  }

  function getGridItems() {
    return avatarSelector.shadowRoot!.querySelectorAll<CrButtonElement>(
        '.avatar');
  }

  function verifyTabIndex(
      items: NodeListOf<CrButtonElement>, tabIndexArr: number[]) {
    assertEquals(items.length, tabIndexArr.length);
    for (let i = 0; i < items.length; i++) {
      assertEquals(items[i]!.tabIndex, tabIndexArr[i]);
    }
  }

  setup(function() {
    avatarSelector = createElement();
    document.body.appendChild(avatarSelector);
  });

  teardown(function() {
    avatarSelector.remove();
  });

  test('Displays avatars', function() {
    assertEquals(3, getGridItems().length);
  });

  test('No avatar is initially selected', function() {
    assertNull(avatarSelector.selectedAvatar);
    getGridItems().forEach(function(item) {
      assertFalse(item.parentElement!.classList.contains('iron-selected'));
    });
  });

  test('No avatar initially selected', async function() {
    const items = getGridItems();
    assertEquals(items.length, 3);
    // First element of the grid should get the focus on 'tab' key.
    verifyTabIndex(items, [0, -1, -1]);

    // Tab key should not move the focus.
    items[0]!.focus();
    pressAndReleaseKeyOn(items[0]!, 9);
    assertEquals(getDeepActiveElement(), items[0]);

    keyDownOn(items[0]!, 39, [], 'ArrowRight');
    assertEquals(getDeepActiveElement(), items[1]);

    items[1]!.click();
    await microtasksFinished();
    assertTrue(items[1]!.parentElement!.classList.contains('iron-selected'));
    verifyTabIndex(items, [-1, 0, -1]);
  });

  test('Avatar already selected', async function() {
    let items = getGridItems();
    verifyTabIndex(items, [0, -1, -1]);
    avatarSelector.avatars = [
      {
        url: 'chrome://avatar1.png',
        label: 'avatar1',
        index: 1,
        selected: false,
        isGaiaAvatar: false,
      },
      {
        url: 'chrome://avatar2.png',
        label: 'avatar2',
        index: 2,
        selected: true,
        isGaiaAvatar: false,
      },
    ];
    await microtasksFinished();
    items = getGridItems();
    assertTrue(items[1]!.parentElement!.classList.contains('iron-selected'));
    verifyTabIndex(items, [-1, 0]);

    items[0]!.click();
    await microtasksFinished();
    assertTrue(items[0]!.parentElement!.classList.contains('iron-selected'));
    verifyTabIndex(items, [0, -1]);
  });

  test('Avatar selected by setting selectedAvatar', async function() {
    const items = getGridItems();
    verifyTabIndex(items, [0, -1, -1]);
    avatarSelector.selectedAvatar = avatarSelector.avatars[1]!;
    await microtasksFinished();
    verifyTabIndex(getGridItems(), [-1, 0, -1]);

    items[0]!.click();
    await microtasksFinished();
    assertTrue(items[0]!.parentElement!.classList.contains('iron-selected'));
    verifyTabIndex(items, [0, -1, -1]);
  });

  test('Can select avatar', async function() {
    const items = getGridItems();

    // Simulate tapping the third avatar.
    items[2]!.click();
    await microtasksFinished();
    assertEquals('chrome://avatar3.png', avatarSelector.selectedAvatar!.url);
    assertFalse(items[0]!.parentElement!.classList.contains('iron-selected'));
    assertFalse(items[1]!.parentElement!.classList.contains('iron-selected'));
    assertTrue(items[2]!.parentElement!.classList.contains('iron-selected'));
  });

  test('selected-avatar-changed fires', async function() {
    const items = getGridItems();
    items[2]!.click();
    const e = await eventToPromise('selected-avatar-changed', avatarSelector);
    assertEquals(avatarSelector.avatars[2], e.detail.value);
  });

  test('sets ignoreModifiedKeyEvents', async function() {
    const grid = avatarSelector.shadowRoot!.querySelector('cr-grid');
    assertTrue(!!grid);

    assertFalse(avatarSelector.ignoreModifiedKeyEvents);
    assertFalse(grid.ignoreModifiedKeyEvents);

    avatarSelector.ignoreModifiedKeyEvents = true;
    await microtasksFinished();
    assertTrue(grid.ignoreModifiedKeyEvents);
  });
});
