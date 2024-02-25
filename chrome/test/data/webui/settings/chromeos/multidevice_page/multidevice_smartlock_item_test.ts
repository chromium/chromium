// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsMultideviceSmartlockItemElement} from 'chrome://os-settings/lazy_load.js';
import {MultiDeviceBrowserProxyImpl, MultiDeviceFeature, MultiDeviceFeatureState, MultiDevicePageContentData, MultiDeviceSettingsMode, Router} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {createFakePageContentData, TestMultideviceBrowserProxy} from './test_multidevice_browser_proxy.js';

suite('<settings-multidevice-smartlock-item>', () => {
  let smartLockItem: SettingsMultideviceSmartlockItemElement;
  let browserProxy: TestMultideviceBrowserProxy;

  /**
   * Sets pageContentData via WebUI Listener and flushes.
   */
  function setPageContentData(newPageContentData: MultiDevicePageContentData) {
    webUIListenerCallback(
        'settings.updateMultidevicePageContentData', newPageContentData);
    flush();
  }

  /**
   * Sets pageContentData to the specified mode. If it is a mode corresponding
   * to a set host, it will set the hostDeviceName to the provided name or else
   * default to HOST_DEVICE.
   * @param newHostDeviceName Overrides default if |newMode|
   *     corresponds to a set host.
   */
  function setHostData(
      newMode: MultiDeviceSettingsMode, newHostDeviceName?: string): void {
    setPageContentData(createFakePageContentData(newMode, newHostDeviceName));
  }

  function setSmartLockState(newState: MultiDeviceFeatureState) {
    setPageContentData(Object.assign(
        {}, smartLockItem.pageContentData, {smartLockState: newState}));
  }

  function setBetterTogetherState(newState: MultiDeviceFeatureState) {
    setPageContentData(Object.assign(
        {}, smartLockItem.pageContentData, {betterTogetherState: newState}));
  }

  /**
   * @param {boolean} enabled Whether to enable or disable the feature.
   * @return {!Promise} Promise which resolves when the state change has been
   *     verified.
   * @private
   */
  async function simulateFeatureStateChangeRequest(enabled: boolean) {
    const token = 'token1';
    smartLockItem.authToken = {
      lifetimeSeconds: 300,
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

  setup(() => {
    browserProxy = new TestMultideviceBrowserProxy();
    MultiDeviceBrowserProxyImpl.setInstanceForTesting(browserProxy);

    smartLockItem =
        document.createElement('settings-multidevice-smartlock-item');

    document.body.appendChild(smartLockItem);
    flush();

    setHostData(MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    setBetterTogetherState(MultiDeviceFeatureState.ENABLED_BY_USER);
    setSmartLockState(MultiDeviceFeatureState.ENABLED_BY_USER);

    return browserProxy.whenCalled('getPageContentData');
  });

  teardown(() => {
    smartLockItem.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('settings row visibile only if host is verified', () => {
    for (const mode of Object.values(MultiDeviceSettingsMode)) {
      setHostData(mode as MultiDeviceSettingsMode);
      setBetterTogetherState(MultiDeviceFeatureState.ENABLED_BY_USER);
      setSmartLockState(MultiDeviceFeatureState.ENABLED_BY_USER);
      const featureItem =
          smartLockItem.shadowRoot!.querySelector('#smartLockItem');
      if (mode === MultiDeviceSettingsMode.HOST_SET_VERIFIED) {
        assert(featureItem);
      } else {
        assertEquals(null, featureItem);
      }
    }
  });

  test('settings row visibile only if feature is supported', () => {
    let featureItem = smartLockItem.shadowRoot!.querySelector('#smartLockItem');
    assert(featureItem);

    setHostData(MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    setSmartLockState(MultiDeviceFeatureState.NOT_SUPPORTED_BY_CHROMEBOOK);
    featureItem = smartLockItem.shadowRoot!.querySelector('#smartLockItem');
    assertEquals(null, featureItem);

    setHostData(MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    setSmartLockState(MultiDeviceFeatureState.NOT_SUPPORTED_BY_PHONE);
    featureItem = smartLockItem.shadowRoot!.querySelector('#smartLockItem');
    assertEquals(null, featureItem);
  });

  test('settings row visibile only if better together suite is enabled', () => {
    let featureItem = smartLockItem.shadowRoot!.querySelector('#smartLockItem');
    assert(featureItem);
    setBetterTogetherState(MultiDeviceFeatureState.DISABLED_BY_USER);
    featureItem = smartLockItem.shadowRoot!.querySelector('#smartLockItem');
    assertEquals(null, featureItem);
  });

  test('feature toggle click event handled', async () => {
    await simulateFeatureStateChangeRequest(false);
    await simulateFeatureStateChangeRequest(true);
  });

});
