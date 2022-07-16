// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {TestLifetimeBrowserProxy} from './test_os_lifetime_browser_proxy.m.js';
// #import {MultiDeviceSettingsMode, MultiDeviceFeature, MultiDeviceFeatureState, MultiDevicePageContentData, MultiDeviceBrowserProxyImpl, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {TestOsResetBrowserProxy} from './test_os_reset_browser_proxy.m.js';
// #import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {TestMultideviceBrowserProxy, createFakePageContentData, HOST_DEVICE} from './test_multidevice_browser_proxy.m.js';
// #import {isChildVisible, waitAfterNextRender} from 'chrome://test/test_util.js';
// import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// clang-format on

suite('Multidevice', function() {
  let smartLockItem = null;
  let browserProxy = null;

  /** @type {Array<settings.MultiDeviceSettingsMode>} */
  let ALL_MODES;

  /** @type {!settings.Route} */
  let initialRoute;

  /**
   * Sets pageContentData via WebUI Listener and flushes.
   * @param {!MultiDevicePageContentData}
   */
  function setPageContentData(newPageContentData) {
    cr.webUIListenerCallback(
        'settings.updateMultidevicePageContentData', newPageContentData);
    Polymer.dom.flush();
  }

  /**
   * Sets pageContentData to the specified mode. If it is a mode corresponding
   * to a set host, it will set the hostDeviceName to the provided name or else
   * default to multidevice.HOST_DEVICE.
   * @param {settings.MultiDeviceSettingsMode} newMode
   * @param {string=} opt_newHostDeviceName Overrides default if |newMode|
   *     corresponds to a set host.
   */
  function setHostData(newMode, opt_newHostDeviceName) {
    setPageContentData(
        multidevice.createFakePageContentData(newMode, opt_newHostDeviceName));
  }

  /**
   * @param {settings.MultiDeviceFeatureState} newState
   */
  function setSmartLockState(newState) {
    setPageContentData(Object.assign(
        {}, smartLockItem.pageContentData, {smartLockState: newState}));
  }

  /**
   * @param {settings.MultiDeviceFeatureState} newState
   */
  function setBetterTogetherState(newState) {
    setPageContentData(Object.assign(
        {}, smartLockItem.pageContentData, {betterTogetherState: newState}));
  }

  /**
   * Clicks an element, and asserts that the route has changed to
   * |expectedRoute|, then navigates back to |initialRoute|.
   * @param {HTMLElement} element. Target of click.
   * @param {?settings.Route} expectedRoute. The expected current route after
   * clicking |element|. If null, then the |initialRoute| is expected.
   */
  function expectRouteOnClick(element, expectedRoute) {
    element.click();
    Polymer.dom.flush();
    if (expectedRoute) {
      assertEquals(
          expectedRoute, settings.Router.getInstance().getCurrentRoute());
      settings.Router.getInstance().navigateTo(initialRoute);
    }
    assertEquals(initialRoute, settings.Router.getInstance().getCurrentRoute());
  }

  /**
   * @param {boolean} enabled Whether to enable or disable the feature.
   * @return {!Promise} Promise which resolves when the state change has been
   *     verified.
   * @private
   */
  function simulateFeatureStateChangeRequest(enabled) {
    const token = 'token1';
    smartLockItem.authToken =
        /** @type{chrome.quickUnlockPrivate} */ {
          lifetimeDuration: 300,
          token: token
        };

    // When the user requets a feature state change, an event with the relevant
    // details is handled.
    smartLockItem.fire(
        'feature-toggle-clicked',
        {feature: MultiDeviceFeature.SMART_LOCK, enabled: enabled});
    Polymer.dom.flush();

    return browserProxy.whenCalled('setFeatureEnabledState').then(params => {
      assertEquals(MultiDeviceFeature.SMART_LOCK, params[0]);
      assertEquals(enabled, params[1]);
      assertEquals(token, params[2]);

      // Reset the resolver so that setFeatureEnabledState() can be called
      // multiple times in a test.
      browserProxy.resetResolver('setFeatureEnabledState');
    });
  }

  suiteSetup(function() {
    ALL_MODES = Object.values(settings.MultiDeviceSettingsMode);
  });

  setup(function() {
    PolymerTest.clearBody();
    browserProxy = new multidevice.TestMultideviceBrowserProxy();
    settings.MultiDeviceBrowserProxyImpl.instance_ = browserProxy;

    smartLockItem =
        document.createElement('settings-multidevice-smartlock-item');
    assertTrue(!!smartLockItem);

    document.body.appendChild(smartLockItem);
    Polymer.dom.flush();

    initialRoute = settings.routes.LOCK_SCREEN;
    setHostData(settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    setBetterTogetherState(settings.MultiDeviceFeatureState.ENABLED_BY_USER);
    setSmartLockState(settings.MultiDeviceFeatureState.ENABLED_BY_USER);

    return browserProxy.whenCalled('getPageContentData');
  });

  teardown(function() {
    smartLockItem.remove();
    settings.Router.getInstance().resetRouteForTesting();
  });

  test('settings row visibile only if host is verified', function() {
    for (const mode of ALL_MODES) {
      setHostData(mode);
      setBetterTogetherState(settings.MultiDeviceFeatureState.ENABLED_BY_USER);
      setSmartLockState(settings.MultiDeviceFeatureState.ENABLED_BY_USER);
      const featureItem = smartLockItem.$$('#smartLockItem');
      if (mode === settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED) {
        assertTrue(!!featureItem);
      } else {
        assertFalse(!!featureItem);
      }
    }
  });

  test('settings row visibile only if feature is supported', function() {
    let featureItem = smartLockItem.$$('#smartLockItem');
    assertTrue(!!featureItem);

    setHostData(settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    setSmartLockState(
        settings.MultiDeviceFeatureState.NOT_SUPPORTED_BY_CHROMEBOOK);
    featureItem = smartLockItem.$$('#smartLockItem');
    assertFalse(!!featureItem);

    setHostData(settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    setSmartLockState(settings.MultiDeviceFeatureState.NOT_SUPPORTED_BY_PHONE);
    featureItem = smartLockItem.$$('#smartLockItem');
    assertFalse(!!featureItem);
  });

  test(
      'settings row visibile only if better together suite is enabled',
      function() {
        let featureItem = smartLockItem.$$('#smartLockItem');
        assertTrue(!!featureItem);
        setBetterTogetherState(
            settings.MultiDeviceFeatureState.DISABLED_BY_USER);
        featureItem = smartLockItem.$$('#smartLockItem');
        assertFalse(!!featureItem);
      });

  test('clicking item with verified host opens subpage', function() {
    const featureItem = smartLockItem.$$('#smartLockItem');
    assertTrue(!!featureItem);
    expectRouteOnClick(
        featureItem.$$('#linkWrapper'), settings.routes.SMART_LOCK);
  });

  test('feature toggle click event handled', function() {
    simulateFeatureStateChangeRequest(false).then(function() {
      simulateFeatureStateChangeRequest(true);
    });
  });
});
