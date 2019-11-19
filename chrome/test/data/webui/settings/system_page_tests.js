// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @const {boolean} */
const HARDWARE_ACCELERATION_AT_STARTUP = true;

/** @implements {settings.SystemPageBrowserProxy} */
class TestSystemPageBrowserProxy extends TestBrowserProxy {
  constructor() {
    super(['showProxySettings']);
  }

  /** @override */
  showProxySettings() {
    this.methodCalled('showProxySettings');
  }

  /** @override */
  wasHardwareAccelerationEnabledAtStartup() {
    return HARDWARE_ACCELERATION_AT_STARTUP;
  }
}

suite('settings system page', function() {
  /** @type {TestSystemPageBrowserProxy} */
  let systemBrowserProxy;

  /** @type {settings.TestLifetimeBrowserProxy} */
  let lifetimeBrowserProxy;

  /** @type {SettingsSystemPageElement} */
  let systemPage;

  setup(function() {
    PolymerTest.clearBody();
    lifetimeBrowserProxy = new settings.TestLifetimeBrowserProxy();
    settings.LifetimeBrowserProxyImpl.instance_ = lifetimeBrowserProxy;
    systemBrowserProxy = new TestSystemPageBrowserProxy();
    settings.SystemPageBrowserProxyImpl.instance_ = systemBrowserProxy;

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
    expectEquals(HARDWARE_ACCELERATION_AT_STARTUP, control.checked);

    // Restart button should be hidden by default.
    expectFalse(!!control.querySelector('cr-button'));

    systemPage.set(
        'prefs.hardware_acceleration_mode.enabled.value',
        !HARDWARE_ACCELERATION_AT_STARTUP);
    Polymer.dom.flush();
    expectNotEquals(HARDWARE_ACCELERATION_AT_STARTUP, control.checked);

    const restart = control.querySelector('cr-button');
    expectTrue(!!restart);  // The "RESTART" button should be showing now.

    restart.click();
    return lifetimeBrowserProxy.whenCalled('restart');
  });

  test('proxy row', function() {
    systemPage.$.proxy.click();
    return systemBrowserProxy.whenCalled('showProxySettings');
  });

  test('proxy row enforcement', function() {
    const control = systemPage.$.proxy;
    const showProxyButton = control.querySelector('cr-icon-button');
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
    Polymer.dom.flush();

    // When managed by extensions, we disable the ability to show proxy
    // settings.
    expectFalse(control.hasAttribute('actionable'));
    expectEquals(null, control.querySelector('cr-policy-pref-indicator'));
    expectFalse(showProxyButton.hidden);

    systemPage.set('prefs.proxy', {
      key: 'proxy',
      type: chrome.settingsPrivate.PrefType.DICTIONARY,
      value: {mode: 'system'},
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
    });
    Polymer.dom.flush();

    // When managed by policy directly, we disable the ability to show proxy
    // settings.
    expectFalse(control.hasAttribute('actionable'));
    expectNotEquals(null, control.querySelector('cr-policy-pref-indicator'));
    expectTrue(showProxyButton.hidden);
  });
});
