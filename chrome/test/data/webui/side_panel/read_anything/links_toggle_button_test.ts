
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {LINK_TOGGLE_BUTTON_ID, LINKS_DISABLED_ICON, LINKS_ENABLED_ICON, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ReadAnythingToolbarElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertStringContains, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {FakeReadingMode} from './fake_reading_mode.js';

suite('LinksToggle', () => {
  let toolbar: ReadAnythingToolbarElement;
  let menuButton: CrIconButtonElement;
  let linksToggled: boolean;

  setup(async () => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;

    linksToggled = false;
    document.addEventListener(ToolbarEvent.LINKS, () => linksToggled = true);
    toolbar = document.createElement('read-anything-toolbar');
    document.body.appendChild(toolbar);
    await microtasksFinished();
    menuButton = toolbar.shadowRoot.querySelector<CrIconButtonElement>(
        '#' + LINK_TOGGLE_BUTTON_ID)!;
  });

  test('by default links are on and button is enabled', () => {
    assertEquals(LINKS_ENABLED_ICON, menuButton.ironIcon);
    assertTrue(chrome.readingMode.linksEnabled);
    assertStringContains('disable links', menuButton.title.toLowerCase());
    assertFalse(menuButton.disabled);
  });

  suite('on first click', () => {
    setup(() => {
      menuButton.click();
      return microtasksFinished();
    });

    test('links are turned off', () => {
      assertEquals(LINKS_DISABLED_ICON, menuButton.ironIcon);
      assertFalse(chrome.readingMode.linksEnabled);
      assertStringContains('enable links', menuButton.title.toLowerCase());
    });

    test('event is propagated', () => {
      assertTrue(linksToggled);
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

      test('links are turned back on', () => {
        assertEquals(LINKS_ENABLED_ICON, menuButton.ironIcon);
        assertTrue(chrome.readingMode.linksEnabled);
        assertStringContains('disable links', menuButton.title.toLowerCase());
      });

      test('event is propagated', () => {
        assertTrue(linksToggled);
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
