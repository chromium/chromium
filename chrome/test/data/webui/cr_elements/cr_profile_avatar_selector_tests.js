// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_profile_avatar_selector/cr_profile_avatar_selector.m.js';

import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {keyDownOn, pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
// clang-format on

/** @fileoverview Suite of tests for cr-profile-avatar-selector. */
suite('cr-profile-avatar-selector', function() {
  /** @type {CrProfileAvatarSelectorElement} */
  let avatarSelector = null;

  /** @return {!CrProfileAvatarSelectorElement} */
  function createElement() {
    const avatarSelector = /** @type {!CrProfileAvatarSelectorElement} */ (
        document.createElement('cr-profile-avatar-selector'));
    avatarSelector.avatars = [
      {
        url: 'chrome://avatar1.png',
        label: 'avatar1',
        index: '1',
        selected: false
      },
      {
        url: 'chrome://avatar2.png',
        label: 'avatar2',
        index: '2',
        selected: false
      },
      {
        url: 'chrome://avatar3.png',
        label: 'avatar3',
        index: '3',
        selected: false
      }
    ];
    return avatarSelector;
  }

  /** @return {!NodeList<!Element>} */
  function getGridItems() {
    return avatarSelector.shadowRoot.querySelectorAll('.avatar');
  }

  function verifyTabIndex(items, tabIndexArr) {
    assertEquals(items.length, tabIndexArr.length);
    for (let i = 0; i < items.length; i++) {
      assertEquals(items[i].tabIndex, tabIndexArr[i]);
    }
  }

  setup(function() {
    avatarSelector = createElement();
    document.body.appendChild(avatarSelector);
    flush();
  });

  teardown(function() {
    avatarSelector.remove();
  });

  test('Displays avatars', function() {
    assertEquals(3, getGridItems().length);
  });

  test('Can update avatars', function() {
    avatarSelector.pop('avatars');
    flush();
    assertEquals(2, getGridItems().length);
  });

  test('No avatar is initially selected', function() {
    assertFalse(!!avatarSelector.selectedAvatar);
    getGridItems().forEach(function(item) {
      assertFalse(item.classList.contains('iron-selected'));
    });
  });

  test('No avatar initially selected', function() {
    const items = getGridItems();
    assertEquals(items.length, 3);
    // First element of the grid should get the focus on 'tab' key.
    verifyTabIndex(items, [0, -1, -1]);

    // Tab key should not move the focus.
    items[0].focus();
    pressAndReleaseKeyOn(items[0], 9);
    assertEquals(getDeepActiveElement(), items[0]);

    keyDownOn(items[0], 39, [], 'ArrowRight');
    assertEquals(getDeepActiveElement(), items[1]);

    items[1].click();
    assertTrue(items[1].classList.contains('iron-selected'));
    verifyTabIndex(items, [-1, 0, -1]);
  });

  test('Avatar already selected', function() {
    let items = getGridItems();
    verifyTabIndex(items, [0, -1, -1]);
    avatarSelector.avatars = [
      {
        url: 'chrome://avatar1.png',
        label: 'avatar1',
        index: '1',
        selected: false
      },
      {
        url: 'chrome://avatar2.png',
        label: 'avatar2',
        index: '2',
        selected: true
      }
    ];
    flush();
    items = getGridItems();
    assertTrue(items[1].classList.contains('iron-selected'));
    verifyTabIndex(items, [-1, 0]);

    items[0].click();
    assertTrue(items[0].classList.contains('iron-selected'));
    verifyTabIndex(items, [0, -1]);
  });

  test('Avatar selected by setting selectedAvatar', async function() {
    const items = getGridItems();
    verifyTabIndex(items, [0, -1, -1]);
    avatarSelector.selectedAvatar = avatarSelector.avatars[1];
    flush();
    verifyTabIndex(getGridItems(), [-1, 0, -1]);

    items[0].click();
    assertTrue(items[0].classList.contains('iron-selected'));
    verifyTabIndex(items, [0, -1, -1]);
  });

  test('Can select avatar', function() {
    const items = getGridItems();

    // Simulate tapping the third avatar.
    items[2].click();
    assertEquals('chrome://avatar3.png', avatarSelector.selectedAvatar.url);
    assertFalse(items[0].classList.contains('iron-selected'));
    assertFalse(items[1].classList.contains('iron-selected'));
    assertTrue(items[2].classList.contains('iron-selected'));
  });

  test('ignores modified key events', function() {
    const selector = avatarSelector.$['avatar-grid'];
    const items = getGridItems();

    items[0].focus();
    assertEquals(getDeepActiveElement(), items[0]);

    keyDownOn(items[0], 39, [], 'ArrowRight');
    assertEquals(getDeepActiveElement(), items[1]);

    keyDownOn(items[0], 37, [], 'ArrowLeft');
    assertEquals(getDeepActiveElement(), items[0]);

    avatarSelector.ignoreModifiedKeyEvents = true;

    keyDownOn(items[0], 39, 'alt', 'ArrowRight');
    assertEquals(getDeepActiveElement(), items[0]);

    keyDownOn(items[0], 39, 'ctrl', 'ArrowRight');
    assertEquals(getDeepActiveElement(), items[0]);

    keyDownOn(items[0], 39, 'meta', 'ArrowRight');
    assertEquals(getDeepActiveElement(), items[0]);

    keyDownOn(items[0], 39, 'shift', 'ArrowRight');
    assertEquals(getDeepActiveElement(), items[0]);

    // Test RTL case.
    selector.dir = 'rtl';
    keyDownOn(items[0], 37, [], 'ArrowLeft');
    assertEquals(getDeepActiveElement(), items[1]);

    keyDownOn(items[0], 37, [], 'ArrowLeft');
    assertEquals(getDeepActiveElement(), items[2]);

    keyDownOn(items[0], 37, [], 'ArrowRight');
    assertEquals(getDeepActiveElement(), items[1]);

    keyDownOn(items[0], 37, [], 'ArrowRight');
    assertEquals(getDeepActiveElement(), items[0]);
  });
});
