// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {ReadAnythingToolbarElement, SettingsMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {stubAnimationFrame} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';

suite('Toolbar Settings Menu', () => {
  let toolbar: ReadAnythingToolbarElement;
  let shadowRoot: ShadowRoot;
  let menuButton: CrIconButtonElement;
  let settingsMenu: SettingsMenuElement;

  async function createToolbar(): Promise<void> {
    toolbar = document.createElement('read-anything-toolbar');
    document.body.appendChild(toolbar);
    await microtasksFinished();
    assertTrue(!!toolbar.shadowRoot);
    shadowRoot = toolbar.shadowRoot;
  }

  function getButton(id: string): CrIconButtonElement|null {
    return shadowRoot.querySelector<CrIconButtonElement>('#' + id);
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    chrome.readingMode.isReadAloudEnabled = true;
    chrome.readingMode.isImmersiveEnabled = true;
    stubAnimationFrame();
    await createToolbar();

    const moreButton = getButton('more');
    assertTrue(!!moreButton);
    menuButton = moreButton;

    settingsMenu = toolbar.$.settingsMenu;
    menuButton.click();

    await microtasksFinished();
  });

  teardown(async () => {
    if (settingsMenu) {
      const lazyMenu = settingsMenu.$.lazyMenu.getIfExists();
      if (lazyMenu && lazyMenu.open) {
        settingsMenu.close();
      }
    }
    await microtasksFinished();
  });

  test('settings is dropdown menu for more', () => {
    assertTrue(settingsMenu.$.lazyMenu.get().open);
  });

  test('settings menu opens submenus on click', () => {
    const actionMenu = settingsMenu.$.lazyMenu.get();
    const menuItems = actionMenu.querySelectorAll('.menu-row');
    const targetItem = menuItems[1] as HTMLElement;
    assertTrue(!!targetItem);
    assertEquals(1, Number.parseInt(targetItem.dataset['index']!));

    targetItem.click();
    assertTrue(toolbar.$.fontMenu.$.menu.$.lazyMenu.get().open);
  });

  test('menus are closed when clicked out of them', () => {
    const actionMenu = settingsMenu.$.lazyMenu.get();
    const menuItems = actionMenu.querySelectorAll('.menu-row');
    const targetItem = menuItems[1] as HTMLElement;
    assertTrue(!!targetItem);
    assertEquals(1, Number.parseInt(targetItem.dataset['index']!));

    targetItem.click();
    assertTrue(toolbar.$.fontMenu.$.menu.$.lazyMenu.get().open);

    menuButton.click();
    assertFalse(actionMenu.open);
    assertFalse(toolbar.$.fontMenu.$.menu.$.lazyMenu.get().open);
  });
});
