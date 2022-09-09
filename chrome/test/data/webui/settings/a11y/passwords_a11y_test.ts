// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PasswordsSectionElement} from 'chrome://settings/lazy_load.js';
import {PasswordManagerImpl, routes, SettingsUiElement} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {createPasswordEntry} from '../passwords_and_autofill_fake_data.js';
import {TestPasswordManagerProxy} from '../test_password_manager_proxy.js';

let passwordsSection: PasswordsSectionElement;
let passwordManager: TestPasswordManagerProxy;
let settingsUi: SettingsUiElement|null = null;

const whenReady = new Promise<void>((resolve) => {
  // Set the URL to be that of specific route to load upon injecting
  // settings-ui. Simply calling
  // settings.Router.getInstance().navigateTo(route) prevents use of mock
  // APIs for fake data.
  window.history.pushState('object or string', 'Test', routes.PASSWORDS.path);

  passwordManager = new TestPasswordManagerProxy();
  PasswordManagerImpl.setInstance(passwordManager);

  settingsUi = document.createElement('settings-ui');

  // The settings section will expand to load the PASSWORDS route (based on
  // the URL set above) once the settings-ui element is attached
  settingsUi!.addEventListener('settings-section-expanded', () => {
    // Passwords section should be loaded before setup is complete.
    passwordsSection =
        settingsUi!.shadowRoot!.querySelector('settings-main')!.shadowRoot!
            .querySelector('settings-basic-page')!.shadowRoot!
            .querySelector('settings-autofill-page')!.shadowRoot!.querySelector(
                'passwords-section')!;
    assertTrue(!!passwordsSection);

    assertEquals(passwordManager, passwordsSection.getPasswordManagerForTest());

    resolve();
  });
});

document.body.appendChild(settingsUi!);
whenReady.then(() => {
  const fakePasswords = [];
  for (let i = 0; i < 10; i++) {
    fakePasswords.push(createPasswordEntry({id: i, username: i.toString()}));
  }
  // Set list of passwords.
  passwordManager.lastCallback.addSavedPasswordListChangedListener!
      (fakePasswords);
  flush();

  assertEquals(10, passwordsSection.savedPasswords.length);
  document.dispatchEvent(new CustomEvent('a11y-setup-complete'));
});
