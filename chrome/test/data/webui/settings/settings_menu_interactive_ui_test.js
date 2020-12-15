// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {pageVisibility} from 'chrome://settings/settings.js';
import {assertEquals} from '../../chai_assert.js';

suite('SettingsMenuInteractiveUITest', () => {
  let settingsMenu;

  setup(() => {
    document.body.innerHTML = '';
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
    assertEquals(settingsMenu.$.people, settingsMenu.shadowRoot.activeElement);

    settingsMenu.pageVisibility = Object.assign({}, pageVisibility || {}, {
      people: false,
      autofill: true,
    });
    settingsMenu.focusFirstItem();
    assertEquals(
        settingsMenu.$.autofill, settingsMenu.shadowRoot.activeElement);
  });
});