
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {IMAGES_DISABLED_ICON, IMAGES_ENABLED_ICON, IMAGES_TOGGLE_BUTTON_ID, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertStringContains, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {FakeReadingMode} from './fake_reading_mode.js';

suite('ImageToggle', () => {
  let toolbar: ReadAnythingToolbarElement;
  let menuButton: CrIconButtonElement;
  let imagesToggled: boolean;

  setup(async () => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    chrome.readingMode.imagesFeatureEnabled = true;

    imagesToggled = false;
    document.addEventListener(ToolbarEvent.IMAGES, () => imagesToggled = true);
    toolbar = document.createElement('read-anything-toolbar');
    document.body.appendChild(toolbar);
    await microtasksFinished();
    menuButton = toolbar.shadowRoot.querySelector<CrIconButtonElement>(
        '#' + IMAGES_TOGGLE_BUTTON_ID)!;
  });

  test('by default images are off and button is enabled', () => {
    assertEquals(IMAGES_DISABLED_ICON, menuButton.ironIcon);
    assertFalse(chrome.readingMode.imagesEnabled);
    assertStringContains('enable images', menuButton.title.toLowerCase());
    assertFalse(menuButton.disabled);
  });

  suite('on first click', () => {
    setup(() => {
      menuButton.click();
      return microtasksFinished();
    });

    test('images are turned on', () => {
      assertEquals(IMAGES_ENABLED_ICON, menuButton.ironIcon);
      assertTrue(chrome.readingMode.imagesEnabled);
      assertStringContains('disable images', menuButton.title.toLowerCase());
    });

    test('event is propagated', () => {
      assertTrue(imagesToggled);
    });

    test('when unpaused, button is disabled', async () => {
      toolbar.isSpeechActive = true;
      await microtasksFinished();
      assertTrue(menuButton.disabled);
    });

    test('when paused, button is enabled', async () => {
      toolbar.isSpeechActive = false;
      await microtasksFinished();
      assertFalse(menuButton.disabled);
    });

    suite('on next click', () => {
      setup(() => {
        menuButton.click();
        return microtasksFinished();
      });

      test('images are turned back off', () => {
        assertEquals(IMAGES_DISABLED_ICON, menuButton.ironIcon);
        assertFalse(chrome.readingMode.imagesEnabled);
        assertStringContains('enable images', menuButton.title.toLowerCase());
      });

      test('event is propagated', () => {
        assertTrue(imagesToggled);
      });

      test('when unpaused, button is disabled', async () => {
        toolbar.isSpeechActive = true;
        await microtasksFinished();
        assertTrue(menuButton.disabled);
      });

      test('when paused, button is enabled', async () => {
        toolbar.isSpeechActive = false;
        await microtasksFinished();
        assertFalse(menuButton.disabled);
      });
    });
  });
});
