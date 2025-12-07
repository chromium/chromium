// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {SettingsMenuElement} from 'chrome://settings/settings.js';
import {loadTimeData, resetPageVisibilityForTesting} from 'chrome://settings/settings.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('SettingsMenuInteractiveUITest', () => {
  let settingsMenu: SettingsMenuElement;

  function createMenu() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    settingsMenu = document.createElement('settings-menu');
    document.body.appendChild(settingsMenu);
  }

  test('focusFirstItem', () => {
    resetPageVisibilityForTesting({
      people: true,
      autofill: true,
    });

    loadTimeData.overrideValues({enableYourSavedInfoSettingsPage: false});
    createMenu();
    settingsMenu.focusFirstItem();
    assertEquals(settingsMenu.$.people, settingsMenu.shadowRoot!.activeElement);

    resetPageVisibilityForTesting({
      people: false,
      autofill: true,
    });

    createMenu();
    settingsMenu.focusFirstItem();
    assertEquals(
        settingsMenu.$.autofill, settingsMenu.shadowRoot!.activeElement);
  });

  test('focusFirstItemWithYourSavedInfoPageOn', () => {
    resetPageVisibilityForTesting({
      people: true,
      yourSavedInfo: true,
    });

    loadTimeData.overrideValues({enableYourSavedInfoSettingsPage: true});
    createMenu();
    settingsMenu.focusFirstItem();
    assertEquals(settingsMenu.$.people, settingsMenu.shadowRoot!.activeElement);

    resetPageVisibilityForTesting({
      people: false,
      yourSavedInfo: true,
    });

    createMenu();
    settingsMenu.focusFirstItem();
    assertEquals(
        settingsMenu.$.yourSavedInfo, settingsMenu.shadowRoot!.activeElement);
  });
});
