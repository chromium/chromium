// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {PasswordManagerImpl, PrefsBrowserProxyImpl} from 'chrome://password-manager/password_manager.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
import {createExceptionEntry, makePasswordManagerPrefs} from './test_util.js';

suite('SettingsSectionTest', function() {
  let prefsProxy: TestPrefsBrowserProxy;
  let passwordManager: TestPasswordManagerProxy;

  setup(function() {
    document.body.innerHTML =
        window.trustedTypes!.emptyHTML as unknown as string;
    prefsProxy = new TestPrefsBrowserProxy();
    PrefsBrowserProxyImpl.setInstance(prefsProxy);
    prefsProxy.prefs = makePasswordManagerPrefs();
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
  });

  test('pref value displayed in the UI', async function() {
    await prefsProxy.setPref('credentials_enable_service', false);

    const settings = document.createElement('settings-section');
    document.body.appendChild(settings);
    await prefsProxy.whenCalled('getPref');

    assertFalse(settings.$.passwordToggle.checked);
    assertTrue(settings.$.autosigninToggle.checked);
  });

  test('clicking the toggle updates corresponding', async function() {
    const settings = document.createElement('settings-section');
    document.body.appendChild(settings);
    await prefsProxy.whenCalled('getPref');

    assertTrue(settings.$.passwordToggle.checked);
    settings.$.passwordToggle.click();
    assertFalse(settings.$.passwordToggle.checked);

    const {key, value} = await prefsProxy.whenCalled('setPref');
    assertEquals('credentials_enable_service', key);
    assertFalse(value);
  });

  test('pref updated externally', async function() {
    const settings = document.createElement('settings-section');
    document.body.appendChild(settings);
    await prefsProxy.whenCalled('getPref');

    assertTrue(settings.$.autosigninToggle.checked);
    prefsProxy.prefs = makePasswordManagerPrefs();
    await prefsProxy.setPref('credentials_enable_autosignin', false);

    assertFalse(settings.$.autosigninToggle.checked);
  });

  test('settings section shows exceptions', async function() {
    const settings = document.createElement('settings-section');
    document.body.appendChild(settings);
    await flushTasks();

    // List is hidden as there are no exceptions.
    assertFalse(isVisible(settings.$.blockedSitesList));

    const exceptions = [
      createExceptionEntry('test.com', 0),
      createExceptionEntry('test2.com', 1),
    ];
    assertTrue(!!passwordManager.listeners.blockedSitesListChangedListener);
    passwordManager.listeners.blockedSitesListChangedListener!(exceptions);
    await flushTasks();

    assertTrue(isVisible(settings.$.blockedSitesList));
    assertDeepEquals(exceptions, settings.$.blockedSitesList.items);
  });
});
