// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MultiDeviceBrowserProxyImpl, MultiDeviceFeature, MultiDeviceFeatureState, MultiDevicePageContentData, MultiDeviceSettingsMode, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

import {createFakePageContentData, TestMultideviceBrowserProxy} from './test_multidevice_browser_proxy.js';

suite('Multidevice', function() {
  let smartLockItem = null;
  let browserProxy = null;

  /** @type {Array<MultiDeviceSettingsMode>} */
  let ALL_MODES;

  /** @type {!Route} */
  let initialRoute;

  /**
   * Sets pageContentData via WebUI Listener and flushes.
   * @param {!MultiDevicePageContentData}
   */
  function setPageContentData(newPageContentData) {
    cr.webUIListenerCallback(
        'settings.updateMultidevicePageContentData', newPageContentData);
    flush();
  }

  /**
   * Sets pageContentData to the specified mode. If it is a mode corresponding
   * to a set host, it will set the hostDeviceName to the provided name or else
   * default to HOST_DEVICE.
   * @param {MultiDeviceSettingsMode} newMode
   * @param {string=} opt_newHostDeviceName Overrides default if |newMode|
   *     corresponds to a set host.
   */
  function setHostData(newMode, opt_newHostDeviceName) {
    setPageContentData(
        createFakePageContentData(newMode, opt_newHostDeviceName));
  }

  /**
   * @param {MultiDeviceFeatureState} newState
   */
  function setSmartLockState(newState) {
    setPageContentData(Object.assign(
        {}, smartLockItem.pageContentData, {smartLockState: newState}));
  }

  /**
   * @param {MultiDeviceFeatureState} newState
   */
  function setBetterTogetherState(newState) {
    setPageContentData(Object.assign(
        {}, smartLockItem.pageContentData, {betterTogetherState: newState}));
  }

  /**
   * Clicks an element, and asserts that the route has changed to
   * |expectedRoute|, then navigates back to |initialRoute|.
   * @param {HTMLElement} element. Target of click.
   * @param {?Route} expectedRoute. The expected current route after
   * clicking |element|. If null, then the |initialRoute| is expected.
   */
  function expectRouteOnClick(element, expectedRoute) {
    element.click();
    flush();
    if (expectedRoute) {
      assertEquals(expectedRoute, Router.getInstance().getCurrentRoute());
      Router.getInstance().navigateTo(initialRoute);
    }
    assertEquals(initialRoute, Router.getInstance().getCurrentRoute());
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
    flush();

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
    ALL_MODES = Object.values(MultiDeviceSettingsMode);
  });

  setup(function() {
    PolymerTest.clearBody();
    browserProxy = new TestMultideviceBrowserProxy();
    MultiDeviceBrowserProxyImpl.instance_ = browserProxy;

    smartLockItem =
        document.createElement('settings-multidevice-smartlock-item');
    assertTrue(!!smartLockItem);

    document.body.appendChild(smartLockItem);
    flush();

    initialRoute = routes.LOCK_SCREEN;
    setHostData(MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    setBetterTogetherState(MultiDeviceFeatureState.ENABLED_BY_USER);
    setSmartLockState(MultiDeviceFeatureState.ENABLED_BY_USER);

    return browserProxy.whenCalled('getPageContentData');
  });

  teardown(function() {
    smartLockItem.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('settings row visibile only if host is verified', function() {
    for (const mode of ALL_MODES) {
      setHostData(mode);
      setBetterTogetherState(MultiDeviceFeatureState.ENABLED_BY_USER);
      setSmartLockState(MultiDeviceFeatureState.ENABLED_BY_USER);
      const featureItem = smartLockItem.$$('#smartLockItem');
      if (mode === MultiDeviceSettingsMode.HOST_SET_VERIFIED) {
        assertTrue(!!featureItem);
      } else {
        assertFalse(!!featureItem);
      }
    }
  });

  test('settings row visibile only if feature is supported', function() {
    let featureItem = smartLockItem.$$('#smartLockItem');
    assertTrue(!!featureItem);

    setHostData(MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    setSmartLockState(MultiDeviceFeatureState.NOT_SUPPORTED_BY_CHROMEBOOK);
    featureItem = smartLockItem.$$('#smartLockItem');
    assertFalse(!!featureItem);

    setHostData(MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    setSmartLockState(MultiDeviceFeatureState.NOT_SUPPORTED_BY_PHONE);
    featureItem = smartLockItem.$$('#smartLockItem');
    assertFalse(!!featureItem);
  });

  test(
      'settings row visibile only if better together suite is enabled',
      function() {
        let featureItem = smartLockItem.$$('#smartLockItem');
        assertTrue(!!featureItem);
        setBetterTogetherState(MultiDeviceFeatureState.DISABLED_BY_USER);
        featureItem = smartLockItem.$$('#smartLockItem');
        assertFalse(!!featureItem);
      });

  test('clicking item with verified host opens subpage', function() {
    const featureItem = smartLockItem.$$('#smartLockItem');
    assertTrue(!!featureItem);
    expectRouteOnClick(featureItem.$$('#linkWrapper'), routes.SMART_LOCK);
  });

  test('feature toggle click event handled', function() {
    simulateFeatureStateChangeRequest(false).then(function() {
      simulateFeatureStateChangeRequest(true);
    });
  });
});
