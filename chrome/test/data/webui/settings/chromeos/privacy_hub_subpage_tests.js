// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../chai.js';

import {PrivacyHubBrowserProxyImpl} from 'chrome://os-settings/chromeos/lazy_load.js';
import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {waitAfterNextRender} from 'chrome://test/test_util.js';

import {assertEquals} from '../../chai_assert.js';
import {TestBrowserProxy} from '../../test_browser_proxy.js';

/** @implements {PrivacyHubBrowserProxy} */
class TestPrivacyHubBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getInitialCameraHardwareToggleState',
      'getInitialMicrophoneHardwareToggleState',
      'getInitialAvailabilityOfMicrophoneForSimpleUsage',
    ]);
    this.cameraToggleIsEnabled = false;
    this.microphoneToggleIsEnabled = false;
    this.microphoneForSimpleUsageAvailable = false;
  }

  /** override */
  getInitialCameraHardwareToggleState() {
    this.methodCalled('getInitialCameraHardwareToggleState');
    return Promise.resolve(this.cameraToggleIsEnabled);
  }

  /** override */
  getInitialMicrophoneHardwareToggleState() {
    this.methodCalled('getInitialMicrophoneHardwareToggleState');
    return Promise.resolve(this.microphoneToggleIsEnabled);
  }

  /** override */
  getInitialAvailabilityOfMicrophoneForSimpleUsage() {
    this.methodCalled('getInitialAvailabilityOfMicrophoneForSimpleUsage');
    return Promise.resolve(this.microphoneForSimpleUsageAvailable);
  }
}


suite('PrivacyHubSubpageTests', function() {
  /** @type {SettingsPrivacyHubPage} */
  let privacyHubSubpage = null;

  /** @type {?TestPrivacyHubBrowserProxy} */
  let privacyHubBrowserProxy = null;

  setup(async () => {
    loadTimeData.overrideValues({
      showPrivacyHub: true,
    });

    privacyHubBrowserProxy = new TestPrivacyHubBrowserProxy();
    PrivacyHubBrowserProxyImpl.setInstanceForTesting(privacyHubBrowserProxy);
    privacyHubBrowserProxy.resetResolver('getInitialCameraHardwareToggleState');

    PolymerTest.clearBody();
    privacyHubSubpage = document.createElement('settings-privacy-hub-page');
    document.body.appendChild(privacyHubSubpage);
  });

  teardown(function() {
    privacyHubSubpage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Deep link to camera toggle on privacy hub', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '1116');

    await privacyHubBrowserProxy.whenCalled(
        'getInitialCameraHardwareToggleState');
    Router.getInstance().navigateTo(routes.PRIVACY_HUB, params);

    flush();

    const deepLinkElement =
        privacyHubSubpage.shadowRoot.querySelector('#cameraToggle')
            .shadowRoot.querySelector('cr-toggle');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Camera toggle should be focused for settingId=1116.');
  });

  test('Deep link to microphone toggle on privacy hub', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '1117');
    Router.getInstance().navigateTo(routes.PRIVACY_HUB, params);

    flush();

    const deepLinkElement =
        privacyHubSubpage.shadowRoot.querySelector('#microphoneToggle')
            .shadowRoot.querySelector('cr-toggle');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Microphone toggle should be focused for settingId=1117.');
  });

  test('Deep link to Geolocation toggle on privacy hub', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '1118');
    Router.getInstance().navigateTo(routes.PRIVACY_HUB, params);

    flush();

    const deepLinkElement =
        privacyHubSubpage.shadowRoot.querySelector('#geolocationToggle')
            .shadowRoot.querySelector('cr-toggle');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Geolocation toggle should be focused for settingId=1118.');
  });

  test('Update camera setting sub-label', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '1116');

    privacyHubBrowserProxy.cameraToggleIsEnabled = false;

    await privacyHubBrowserProxy.whenCalled(
        'getInitialCameraHardwareToggleState');
    Router.getInstance().navigateTo(routes.PRIVACY_HUB, params);

    flush();

    const subLabel = privacyHubSubpage.shadowRoot.querySelector('#cameraToggle')
                         .shadowRoot.querySelector('#sub-label-text');

    await waitAfterNextRender(subLabel);

    chai.assert.match(
        subLabel.textContent, /^\s*$/,
        'The sublabel should only consist of whitespace');

    webUIListenerCallback('camera-hardware-toggle-changed', true);

    await waitAfterNextRender(subLabel);

    chai.assert.match(
        subLabel.textContent,
        /^\s*Internal camera deactivated by hardware switch\s*$/,
        'The sublabel should contain the hint about the internal camera');

    webUIListenerCallback('camera-hardware-toggle-changed', false);

    await waitAfterNextRender(subLabel);

    chai.assert.match(
        subLabel.textContent, /^\s*$/,
        'The sublabel should only consist of whitespace');
  });

  test('Update microphone setting sub-label', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '1117');

    privacyHubBrowserProxy.microphoneToggleIsEnabled = false;
    privacyHubBrowserProxy.microphoneForSimpleUsageAvailable = false;

    await privacyHubBrowserProxy.whenCalled(
        'getInitialMicrophoneHardwareToggleState');
    await privacyHubBrowserProxy.whenCalled(
        'getInitialAvailabilityOfMicrophoneForSimpleUsage');
    Router.getInstance().navigateTo(routes.PRIVACY_HUB, params);

    flush();

    const subLabel =
        privacyHubSubpage.shadowRoot.querySelector('#microphoneToggle')
            .shadowRoot.querySelector('#sub-label-text');

    await waitAfterNextRender(subLabel);

    chai.assert.match(
        subLabel.textContent, /^\s*No microphone connected\s*$/,
        'The sublabel should contain the hint about no microphone being ' +
            'connected.');

    webUIListenerCallback(
        'availability-of-microphone-for-simple-usage-changed', true);
    webUIListenerCallback('microphone-hardware-toggle-changed', true);

    await waitAfterNextRender(subLabel);

    chai.assert.match(
        subLabel.textContent,
        /^\s*All microphones disabled by devices hardware switch\s*$/,
        'The sublabel should contain the hint about the microphone hardware ' +
            'switch being active.');


    webUIListenerCallback(
        'availability-of-microphone-for-simple-usage-changed', true);
    webUIListenerCallback('microphone-hardware-toggle-changed', false);

    await waitAfterNextRender(subLabel);

    chai.assert.match(
        subLabel.textContent, /^\s*$/,
        'The sublabel should only consist of whitespace');

    webUIListenerCallback(
        'availability-of-microphone-for-simple-usage-changed', false);
    webUIListenerCallback('microphone-hardware-toggle-changed', true);

    await waitAfterNextRender(subLabel);

    chai.assert.match(
        subLabel.textContent,
        /^\s*All microphones disabled by devices hardware switch\s*$/,
        'The sublabel should contain the hint about the microphone hardware ' +
            'switch being active.');
  });

  test('Suggested content, pref disabled', async () => {
    privacyHubSubpage = document.createElement('settings-privacy-hub-page');
    document.body.appendChild(privacyHubSubpage);
    flush();

    // The default state of the pref is disabled.
    const suggestedContent = assert(
        privacyHubSubpage.shadowRoot.querySelector('#suggested-content'));
    assertFalse(suggestedContent.checked);
  });

  test('Suggested content, pref enabled', async () => {
    // Update the backing pref to enabled.
    privacyHubSubpage.prefs = {
      'settings': {
        'suggested_content_enabled': {
          value: true,
        },
      },
    };

    flush();

    // The checkbox reflects the updated pref state.
    const suggestedContent = assert(
        privacyHubSubpage.shadowRoot.querySelector('#suggested-content'));
    assertTrue(suggestedContent.checked);
  });
});
