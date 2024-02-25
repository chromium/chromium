// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {SettingsMenuElement} from 'chrome://settings/settings.js';
import {pageVisibility} from 'chrome://settings/settings.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('SettingsMenuInteractiveUITest', () => {
  let settingsMenu: SettingsMenuElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    settingsMenu = document.createElement('settings-menu');
    settingsMenu.pageVisibility = pageVisibility;
    document.body.appendChild(settingsMenu);
  });

  test('focusFirstItem', () => {
    settingsMenu.pageVisibility = Object.assign({}, pageVisibility || {}, {
      people: true,
      autofill: true,
    });
    settingsMenu.focusFirstItem();
    assertEquals(settingsMenu.$.people, settingsMenu.shadowRoot!.activeElement);

    settingsMenu.pageVisibility = Object.assign({}, pageVisibility || {}, {
      people: false,
      autofill: true,
    });
    settingsMenu.focusFirstItem();
    assertEquals(
        settingsMenu.$.autofill, settingsMenu.shadowRoot!.activeElement);
  });
});
