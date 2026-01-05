// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {SettingsMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

suite('SettingsMenuElement', () => {
  let settingsMenu: SettingsMenuElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    settingsMenu = document.createElement('settings-menu');
    document.body.appendChild(settingsMenu);
    settingsMenu.$.lazyMenu.get();
  });

  test('click outside fires close-all-menus event', () => {
    settingsMenu.open(document.body);

    let closeWasCalled = false;
    document.addEventListener(
        ToolbarEvent.CLOSE_ALL_MENUS, () => closeWasCalled = true);

    document.body.dispatchEvent(new MouseEvent('click', {
      bubbles: true,
      composed: true,
      cancelable: true,
      view: window,
    }));
    assertTrue(
        closeWasCalled,
        'Clicking outside should fire the close-all-menus event');
    assertFalse(settingsMenu.$.lazyMenu.get().open);
  });

  test('click inside does NOT fires close-all-menus event', () => {
    settingsMenu.open(document.body);

    let closeWasCalled = false;
    document.addEventListener(
        ToolbarEvent.CLOSE_ALL_MENUS, () => closeWasCalled = true);

    const internalMenu = settingsMenu.$.lazyMenu.get();
    internalMenu.dispatchEvent(new MouseEvent('click', {
      bubbles: true,
      composed: true,
      cancelable: true,
      view: window,
    }));
    assertFalse(
        closeWasCalled,
        'Clicking inside should NOT fire the close-all-menus event');
  });
});
