// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {SettingsSystemPageElement, SystemPageBrowserProxy, SystemPageBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {LifetimeBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {TestLifetimeBrowserProxy} from './test_lifetime_browser_proxy.js';

// clang-format on

const HARDWARE_ACCELERATION_AT_STARTUP: boolean = true;

class TestSystemPageBrowserProxy extends TestBrowserProxy implements
    SystemPageBrowserProxy {
  constructor() {
    super(['showProxySettings']);
  }

  showProxySettings() {
    this.methodCalled('showProxySettings');
  }

  wasHardwareAccelerationEnabledAtStartup() {
    return HARDWARE_ACCELERATION_AT_STARTUP;
  }
}

suite('settings system page', function() {
  let systemBrowserProxy: TestSystemPageBrowserProxy;
  let lifetimeBrowserProxy: TestLifetimeBrowserProxy;
  let systemPage: SettingsSystemPageElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    lifetimeBrowserProxy = new TestLifetimeBrowserProxy();
    LifetimeBrowserProxyImpl.setInstance(lifetimeBrowserProxy);
    systemBrowserProxy = new TestSystemPageBrowserProxy();
    SystemPageBrowserProxyImpl.setInstance(systemBrowserProxy);

    systemPage = document.createElement('settings-system-page');
    systemPage.set('prefs', {
      background_mode: {
        enabled: {
          key: 'background_mode.enabled',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
      },
      hardware_acceleration_mode: {
        enabled: {
          key: 'hardware_acceleration_mode.enabled',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: HARDWARE_ACCELERATION_AT_STARTUP,
        },
      },
      proxy: {
        key: 'proxy',
        type: chrome.settingsPrivate.PrefType.DICTIONARY,
        value: {mode: 'system'},
      },
    });
    document.body.appendChild(systemPage);
  });

  teardown(function() {
    systemPage.remove();
  });

  test('restart button', function() {
    const control = systemPage.$.hardwareAcceleration;
    assertEquals(HARDWARE_ACCELERATION_AT_STARTUP, control.checked);

    // Restart button should be hidden by default.
    assertFalse(!!control.querySelector('cr-button'));

    systemPage.set(
        'prefs.hardware_acceleration_mode.enabled.value',
        !HARDWARE_ACCELERATION_AT_STARTUP);
    flush();
    assertNotEquals(HARDWARE_ACCELERATION_AT_STARTUP, control.checked);

    const restart = control.querySelector('cr-button');
    assertTrue(!!restart);  // The "RESTART" button should be showing now.

    restart!.click();
    return lifetimeBrowserProxy.whenCalled('restart');
  });

  test('proxy row', function() {
    systemPage.$.proxy.click();
    return systemBrowserProxy.whenCalled('showProxySettings');
  });

  test('proxy row enforcement', function() {
    const control = systemPage.$.proxy;
    const showProxyButton = control.querySelector('cr-icon-button')!;
    assertTrue(control.hasAttribute('actionable'));
    assertEquals(null, control.querySelector('cr-policy-pref-indicator'));
    assertFalse(showProxyButton.hidden);

    systemPage.set('prefs.proxy', {
      key: 'proxy',
      type: chrome.settingsPrivate.PrefType.DICTIONARY,
      value: {mode: 'system'},
      controlledBy: chrome.settingsPrivate.ControlledBy.EXTENSION,
      extensionId: 'blah',
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
    });
    flush();

    // When managed by extensions, we disable the ability to show proxy
    // settings.
    assertFalse(control.hasAttribute('actionable'));
    assertEquals(null, control.querySelector('cr-policy-pref-indicator'));
    assertTrue(showProxyButton.hidden);

    systemPage.set('prefs.proxy', {
      key: 'proxy',
      type: chrome.settingsPrivate.PrefType.DICTIONARY,
      value: {mode: 'system'},
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
    });
    flush();

    // When managed by policy directly, we disable the ability to show proxy
    // settings.
    assertFalse(control.hasAttribute('actionable'));
    assertNotEquals(null, control.querySelector('cr-policy-pref-indicator'));
    assertTrue(showProxyButton.hidden);
  });
});
