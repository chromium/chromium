// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {PrefsBrowserProxyImpl} from 'chrome://password-manager/password_manager.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
import {makePasswordManagerPrefs} from './test_util.js';

suite('SettingsSectionTest', function() {
  let prefsProxy: TestPrefsBrowserProxy;

  setup(function() {
    document.body.innerHTML =
        window.trustedTypes!.emptyHTML as unknown as string;
    prefsProxy = new TestPrefsBrowserProxy();
    PrefsBrowserProxyImpl.setInstance(prefsProxy);
    prefsProxy.prefs = makePasswordManagerPrefs();
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
});
