// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {MultiDeviceBrowserProxyImpl, MultiDeviceFeature, MultiDeviceFeatureState, MultiDeviceSettingsMode, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

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
    webUIListenerCallback(
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
      assertEquals(expectedRoute, Router.getInstance().currentRoute);
      Router.getInstance().navigateTo(initialRoute);
    }
    assertEquals(initialRoute, Router.getInstance().currentRoute);
  }

  /**
   * @param {boolean} enabled Whether to enable or disable the feature.
   * @return {!Promise} Promise which resolves when the state change has been
   *     verified.
   * @private
   */
  async function simulateFeatureStateChangeRequest(enabled) {
    const token = 'token1';
    smartLockItem.authToken =
        /** @type{chrome.quickUnlockPrivate} */ {
          lifetimeDuration: 300,
          token: token,
        };

    // When the user requets a feature state change, an event with the relevant
    // details is handled.
    const featureToggleClickedEvent =
        new CustomEvent('feature-toggle-clicked', {
          bubbles: true,
          composed: true,
          detail: {feature: MultiDeviceFeature.SMART_LOCK, enabled},
        });
    smartLockItem.dispatchEvent(featureToggleClickedEvent);
    flush();

    const params = await browserProxy.whenCalled('setFeatureEnabledState');
    assertEquals(MultiDeviceFeature.SMART_LOCK, params[0]);
    assertEquals(enabled, params[1]);
    assertEquals(token, params[2]);

    // Reset the resolver so that setFeatureEnabledState() can be called
    // multiple times in a test.
    browserProxy.resetResolver('setFeatureEnabledState');
  }

  /**
   * @param {?boolean} isSmartLockSignInRemoved Whether to enable or disable the
   *     isSmartLockSignInRemoved flag.
   * @private
   * TODO(b/227674947): When Sign in with Smart Lock is removed, this function
   * can be replaced with a normal setup() function that will automatically run
   * before each test and not require a param.
   */
  function initializeElement(isSmartLockSignInRemoved) {
    loadTimeData.overrideValues(
        {'isSmartLockSignInRemoved': !!isSmartLockSignInRemoved});
    PolymerTest.clearBody();
    browserProxy = new TestMultideviceBrowserProxy();
    MultiDeviceBrowserProxyImpl.setInstanceForTesting(browserProxy);

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
  }

  suiteSetup(function() {
    ALL_MODES = Object.values(MultiDeviceSettingsMode);
  });

  teardown(function() {
    if (smartLockItem) {
      smartLockItem.remove();
    }
    Router.getInstance().resetRouteForTesting();
  });

  test('settings row visibile only if host is verified', function() {
    initializeElement();
    for (const mode of ALL_MODES) {
      setHostData(mode);
      setBetterTogetherState(MultiDeviceFeatureState.ENABLED_BY_USER);
      setSmartLockState(MultiDeviceFeatureState.ENABLED_BY_USER);
      const featureItem =
          smartLockItem.shadowRoot.querySelector('#smartLockItem');
      if (mode === MultiDeviceSettingsMode.HOST_SET_VERIFIED) {
        assertTrue(!!featureItem);
      } else {
        assertFalse(!!featureItem);
      }
    }
  });

  test('settings row visibile only if feature is supported', function() {
    initializeElement();
    let featureItem = smartLockItem.shadowRoot.querySelector('#smartLockItem');
    assertTrue(!!featureItem);

    setHostData(MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    setSmartLockState(MultiDeviceFeatureState.NOT_SUPPORTED_BY_CHROMEBOOK);
    featureItem = smartLockItem.shadowRoot.querySelector('#smartLockItem');
    assertFalse(!!featureItem);

    setHostData(MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    setSmartLockState(MultiDeviceFeatureState.NOT_SUPPORTED_BY_PHONE);
    featureItem = smartLockItem.shadowRoot.querySelector('#smartLockItem');
    assertFalse(!!featureItem);
  });

  test(
      'settings row visibile only if better together suite is enabled',
      function() {
        initializeElement();
        let featureItem =
            smartLockItem.shadowRoot.querySelector('#smartLockItem');
        assertTrue(!!featureItem);
        setBetterTogetherState(MultiDeviceFeatureState.DISABLED_BY_USER);
        featureItem = smartLockItem.shadowRoot.querySelector('#smartLockItem');
        assertFalse(!!featureItem);
      });

  test('feature toggle click event handled', async function() {
    initializeElement();
    await simulateFeatureStateChangeRequest(false);
    await simulateFeatureStateChangeRequest(true);
  });

  test('SmartLockSignInRemoved flag removes subpage', async function() {
    initializeElement(/*isSmartLockSignInRemoved=*/ true);
    const featureItem =
        smartLockItem.shadowRoot.querySelector('#smartLockItem');
    assertTrue(!!featureItem);
    assertEquals(undefined, featureItem.subpageRoute);
  });
});
