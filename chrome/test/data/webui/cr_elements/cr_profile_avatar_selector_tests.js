// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for cr-profile-avatar-selector. */
cr.define('cr_profile_avatar_selector', function() {
  const TestNames = {
    Basic: 'basic',
    Focus: 'Ignores modified key events',
  };

  function registerTests() {
    suite('cr-profile-avatar-selector', function() {
      /** @type {CrProfileAvatarSelectorElement} */
      let avatarSelector = null;

      /** @return {CrProfileAvatarSelectorElement} */
      function createElement() {
        const avatarSelector =
            document.createElement('cr-profile-avatar-selector');
        avatarSelector.avatars = [
          {url: 'chrome://avatar1.png', label: 'avatar1'},
          {url: 'chrome://avatar2.png', label: 'avatar2'},
          {url: 'chrome://avatar3.png', label: 'avatar3'}
        ];
        return avatarSelector;
      }

      function getGridItems() {
        return avatarSelector.$['avatar-grid'].querySelectorAll('.avatar');
      }

      setup(function() {
        avatarSelector = createElement();
        document.body.appendChild(avatarSelector);
        Polymer.dom.flush();
      });

      teardown(function() {
        avatarSelector.remove();
      });

      // Run in CrElementsProfileAvatarSelectorTest.All as a browser_test.
      suite(TestNames.Basic, function() {
        test('Displays avatars', function() {
          assertEquals(3, getGridItems().length);
        });

        test('Can update avatars', function() {
          avatarSelector.pop('avatars');
          Polymer.dom.flush();
          assertEquals(2, getGridItems().length);
        });

        test('No avatar is initially selected', function() {
          assertFalse(!!avatarSelector.selectedAvatar);
          getGridItems().forEach(function(item) {
            assertFalse(item.classList.contains('iron-selected'));
          });
        });

        test('Can select avatar', function() {
          const items = getGridItems();

          // Simulate tapping the third avatar.
          items[2].click();
          assertEquals(
              'chrome://avatar3.png', avatarSelector.selectedAvatar.url);
          assertFalse(items[0].classList.contains('iron-selected'));
          assertFalse(items[1].classList.contains('iron-selected'));
          assertTrue(items[2].classList.contains('iron-selected'));
        });
      });

      // Run in CrElementsProfileAvatarSelectorFocusTest.All as an
      // interactive_ui_test.
      test(TestNames.Focus, function() {
        const selector = avatarSelector.$['avatar-grid'];
        const items = getGridItems();

        items[0].focus();
        assertEquals(getDeepActiveElement(), items[0]);

        MockInteractions.keyDownOn(items[0], 39, [], 'ArrowRight');
        assertEquals(getDeepActiveElement(), items[1]);

        MockInteractions.keyDownOn(items[0], 37, [], 'ArrowLeft');
        assertEquals(getDeepActiveElement(), items[0]);

        avatarSelector.ignoreModifiedKeyEvents = true;

        MockInteractions.keyDownOn(items[0], 39, 'alt', 'ArrowRight');
        assertEquals(getDeepActiveElement(), items[0]);

        MockInteractions.keyDownOn(items[0], 39, 'ctrl', 'ArrowRight');
        assertEquals(getDeepActiveElement(), items[0]);

        MockInteractions.keyDownOn(items[0], 39, 'meta', 'ArrowRight');
        assertEquals(getDeepActiveElement(), items[0]);

        MockInteractions.keyDownOn(items[0], 39, 'shift', 'ArrowRight');
        assertEquals(getDeepActiveElement(), items[0]);

        // Test RTL case.
        selector.dir = 'rtl';
        MockInteractions.keyDownOn(items[0], 37, [], 'ArrowLeft');
        assertEquals(getDeepActiveElement(), items[1]);

        MockInteractions.keyDownOn(items[0], 37, [], 'ArrowLeft');
        assertEquals(getDeepActiveElement(), items[2]);

        MockInteractions.keyDownOn(items[0], 37, [], 'ArrowRight');
        assertEquals(getDeepActiveElement(), items[1]);

        MockInteractions.keyDownOn(items[0], 37, [], 'ArrowRight');
        assertEquals(getDeepActiveElement(), items[0]);
      });
    });
  }

  return {
    registerTests: registerTests,
    TestNames: TestNames,
  };
});
