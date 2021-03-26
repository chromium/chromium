// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {TestLifetimeBrowserProxy} from './test_os_lifetime_browser_proxy.m.js';
// #import {MultiDeviceSettingsMode, MultiDeviceFeature, MultiDeviceFeatureState, MultiDevicePageContentData, MultiDeviceBrowserProxyImpl, PhoneHubNotificationAccessStatus, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {TestOsResetBrowserProxy} from './test_os_reset_browser_proxy.m.js';
// #import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {TestMultideviceBrowserProxy, createFakePageContentData, HOST_DEVICE} from './test_multidevice_browser_proxy.m.js';
// #import {isChildVisible, waitAfterNextRender} from 'chrome://test/test_util.m.js';
// import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// clang-format on

suite('Multidevice', function() {
  let multidevicePage = null;
  let browserProxy = null;
  let ALL_MODES;

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
  function setSuiteState(newState) {
    setPageContentData(Object.assign(
        {}, multidevicePage.pageContentData, {betterTogetherState: newState}));
  }

  /**
   * @param {settings.MultiDeviceFeatureState} newState
   */
  function setSmartLockState(newState) {
    setPageContentData(Object.assign(
        {}, multidevicePage.pageContentData, {smartLockState: newState}));
  }

  /**
   * @param {settings.MultiDeviceFeatureState} newState
   */
  function setPhoneHubNotificationsState(newState) {
    setPageContentData(Object.assign(
        {}, multidevicePage.pageContentData,
        {phoneHubNotificationsState: newState}));
  }

  /**
   * @param {boolean} accessGranted
   */
  function setPhoneHubNotificationAccessGranted(accessGranted) {
    const accessState = accessGranted ?
        settings.PhoneHubNotificationAccessStatus.ACCESS_GRANTED :
        settings.PhoneHubNotificationAccessStatus.AVAILABLE_BUT_NOT_GRANTED;
    setPageContentData(Object.assign(
        {}, multidevicePage.pageContentData,
        {notificationAccessStatus: accessState}));
  }

  /**
   * @param {!settings.MultiDeviceFeature} feature The feature to change.
   * @param {boolean} enabled Whether to enable or disable the feature.
   * @param {boolean} authRequired Whether authentication is required for the
   *     change.
   * @return {!Promise} Promise which resolves when the state change has been
   *     verified.
   * @private
   */
  function simulateFeatureStateChangeRequest(feature, enabled, authRequired) {
    // When the user requets a feature state change, an event with the relevant
    // details is handled.
    multidevicePage.fire(
        'feature-toggle-clicked', {feature: feature, enabled: enabled});
    Polymer.dom.flush();

    if (authRequired) {
      assertTrue(multidevicePage.showPasswordPromptDialog_);
      // Simulate the user entering a valid password, then closing the dialog.
      multidevicePage.$$('#multidevicePasswordPrompt').fire('token-obtained', {
        token: 'validAuthToken',
        lifetimeSeconds: 300
      });
      // Simulate closing the password prompt dialog
      multidevicePage.$$('#multidevicePasswordPrompt').fire('close');
      Polymer.dom.flush();
    }

    if (enabled &&
        feature === settings.MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS) {
      const accessDialog = multidevicePage.$$(
          'settings-multidevice-notification-access-setup-dialog');
      assertEquals(
          !!accessDialog,
          multidevicePage.pageContentData.notificationAccessStatus ===
              settings.PhoneHubNotificationAccessStatus
                  .AVAILABLE_BUT_NOT_GRANTED);
      return;
    }

    assertFalse(multidevicePage.showPasswordPromptDialog_);
    return browserProxy.whenCalled('setFeatureEnabledState').then(params => {
      assertEquals(feature, params[0]);
      assertEquals(enabled, params[1]);

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

    loadTimeData.overrideValues({
      isNearbyShareSupported: true,
    });

    multidevicePage = document.createElement('settings-multidevice-page');
    assertTrue(!!multidevicePage);

    multidevicePage.prefs = {
      'nearby_sharing': {
        'onboarding_complete': {
          value: false,
        },
        'enabled': {
          value: false,
        },
      },
    };

    document.body.appendChild(multidevicePage);
    Polymer.dom.flush();

    return browserProxy.whenCalled('getPageContentData');
  });

  teardown(function() {
    multidevicePage.remove();
    settings.Router.getInstance().resetRouteForTesting();
  });

  const getLabel = () => {
    return multidevicePage.$$('#multidevice-label').textContent.trim();
  };

  const getSubpage = () => multidevicePage.$$('settings-multidevice-subpage');

  test('clicking setup shows multidevice setup dialog', function() {
    setHostData(settings.MultiDeviceSettingsMode.NO_HOST_SET);
    const button = multidevicePage.$$('cr-button');
    assertTrue(!!button);
    button.click();
    return browserProxy.whenCalled('showMultiDeviceSetupDialog');
  });

  test('Deep link to multidevice setup', async () => {
    loadTimeData.overrideValues({
      isDeepLinkingEnabled: true,
    });
    setHostData(settings.MultiDeviceSettingsMode.NO_HOST_SET);

    const params = new URLSearchParams;
    params.append('settingId', '200');
    settings.Router.getInstance().navigateTo(
        settings.routes.MULTIDEVICE, params);

    Polymer.dom.flush();

    const deepLinkElement = multidevicePage.$$('cr-button');
    await test_util.waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Setup multidevice button should be focused for settingId=200.');
  });

  test('Open notification access setup dialog route param', async () => {
    settings.Router.getInstance().navigateTo(
        settings.routes.MULTIDEVICE_FEATURES,
        new URLSearchParams('showNotificationAccessSetupDialog=true'));

    PolymerTest.clearBody();
    browserProxy = new multidevice.TestMultideviceBrowserProxy();
    settings.MultiDeviceBrowserProxyImpl.instance_ = browserProxy;
    browserProxy.data.notificationAccessStatus =
        settings.PhoneHubNotificationAccessStatus.AVAILABLE_BUT_NOT_GRANTED;

    multidevicePage = document.createElement('settings-multidevice-page');
    assertTrue(!!multidevicePage);

    document.body.appendChild(multidevicePage);
    await browserProxy.whenCalled('getPageContentData');

    Polymer.dom.flush();
    assertTrue(!!multidevicePage.$$(
        'settings-multidevice-notification-access-setup-dialog'));

    // Close the dialog.
    multidevicePage.showNotificationAccessSetupDialog_ = false;
    Polymer.dom.flush();

    // A change in pageContentData will not cause the notification access
    // setup dialog to reappaear
    setPageContentData({});
    Polymer.dom.flush();

    assertFalse(!!multidevicePage.$$(
        'settings-multidevice-notification-access-setup-dialog'));
  });

  test('headings render based on mode and host', function() {
    for (const mode of ALL_MODES) {
      setHostData(mode);
      assertEquals(
          multidevicePage.isHostSet(), getLabel() === multidevice.HOST_DEVICE);
    }
  });

  test('changing host device changes header', function() {
    setHostData(settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    assertEquals(getLabel(), multidevice.HOST_DEVICE);
    const anotherHost = 'Super Duper ' + multidevice.HOST_DEVICE;
    setHostData(
        settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED, anotherHost);
    assertEquals(getLabel(), anotherHost);
  });

  test('item is actionable if and only if a host is set', function() {
    for (const mode of ALL_MODES) {
      setHostData(mode);
      assertEquals(
          multidevicePage.isHostSet(),
          !!multidevicePage.$$('.link-wrapper').hasAttribute('actionable'));
    }
  });

  test(
      'clicking item with verified host opens subpage with features',
      function() {
        setHostData(settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED);
        assertFalse(!!getSubpage());
        multidevicePage.$$('.link-wrapper').click();
        assertTrue(!!getSubpage());
        assertTrue(!!getSubpage().$$('settings-multidevice-feature-item'));
      });

  test(
      'clicking item with unverified set host opens subpage without features',
      function() {
        setHostData(
            settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
            multidevice.HOST_DEVICE);
        assertFalse(!!getSubpage());
        multidevicePage.$$('.link-wrapper').click();
        assertTrue(!!getSubpage());
        assertFalse(!!getSubpage().$$('settings-multidevice-feature-item'));
      });

  test('policy prohibited suite shows policy indicator', function() {
    setHostData(settings.MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS);
    assertFalse(!!multidevicePage.$$('cr-policy-indicator'));
    // Prohibit suite by policy.
    setSuiteState(settings.MultiDeviceFeatureState.PROHIBITED_BY_POLICY);
    assertTrue(!!multidevicePage.$$('cr-policy-indicator'));
    // Reallow suite.
    setSuiteState(settings.MultiDeviceFeatureState.DISABLED_BY_USER);
    assertFalse(!!multidevicePage.$$('cr-policy-indicator'));
  });

  test('Phone hub notification access setup dialog', () => {
    setPhoneHubNotificationsState(
        settings.MultiDeviceFeatureState.DISABLED_BY_USER);
    assertFalse(!!multidevicePage.$$(
        'settings-multidevice-notification-access-setup-dialog'));

    setPhoneHubNotificationAccessGranted(false);
    simulateFeatureStateChangeRequest(
        settings.MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
        /*enabled=*/ true, /*authRequired=*/ false);

    // Close the dialog.
    multidevicePage.showNotificationAccessSetupDialog_ = false;

    setPhoneHubNotificationAccessGranted(false);
    simulateFeatureStateChangeRequest(
        settings.MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
        /*enabled=*/ false, /*authRequired=*/ false);

    setPhoneHubNotificationAccessGranted(true);
    simulateFeatureStateChangeRequest(
        settings.MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
        /*enabled=*/ true, /*authRequired=*/ false);

    multidevicePage.pageContentData.isNotificationAccessGranted = true;
    simulateFeatureStateChangeRequest(
        settings.MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
        /*enabled=*/ false, /*authRequired=*/ false);
  });

  test('Disabling features never requires authentication', () => {
    const Feature = settings.MultiDeviceFeature;

    const disableFeatureFn = feature => {
      return simulateFeatureStateChangeRequest(
          feature, false /* enabled */, false /* authRequired */);
    };

    return disableFeatureFn(Feature.BETTER_TOGETHER_SUITE)
        .then(() => {
          return disableFeatureFn(Feature.INSTANT_TETHERING);
        })
        .then(() => {
          return disableFeatureFn(Feature.MESSAGES);
        })
        .then(() => {
          return disableFeatureFn(Feature.SMART_LOCK);
        });
  });

  test('Enabling some features requires authentication; others do not', () => {
    const Feature = settings.MultiDeviceFeature;
    const FeatureState = settings.MultiDeviceFeatureState;

    const enableFeatureWithoutAuthFn = feature => {
      return simulateFeatureStateChangeRequest(
          feature, true /* enabled */, false /* authRequired */);
    };
    const enableFeatureWithAuthFn = feature => {
      return simulateFeatureStateChangeRequest(
          feature, true /* enabled */, true /* authRequired */);
    };

    // Start out with SmartLock being disabled by the user. This means that
    // the first attempt to enable BETTER_TOGETHER_SUITE below will not
    // require authentication.
    setSmartLockState(FeatureState.DISABLED_BY_USER);

    // INSTANT_TETHERING requires no authentication.
    return enableFeatureWithoutAuthFn(Feature.INSTANT_TETHERING)
        .then(() => {
          // MESSAGES requires no authentication.
          return enableFeatureWithoutAuthFn(Feature.MESSAGES);
        })
        .then(() => {
          // BETTER_TOGETHER_SUITE requires no authentication normally.
          return enableFeatureWithoutAuthFn(Feature.BETTER_TOGETHER_SUITE);
        })
        .then(() => {
          // BETTER_TOGETHER_SUITE requires authentication when SmartLock's
          // state is UNAVAILABLE_SUITE_DISABLED.
          setSmartLockState(FeatureState.UNAVAILABLE_SUITE_DISABLED);
          return enableFeatureWithAuthFn(Feature.BETTER_TOGETHER_SUITE);
        })
        .then(() => {
          // BETTER_TOGETHER_SUITE requires authentication when SmartLock's
          // state is UNAVAILABLE_INSUFFICIENT_SECURITY.
          setSmartLockState(FeatureState.UNAVAILABLE_INSUFFICIENT_SECURITY);
          return enableFeatureWithAuthFn(Feature.BETTER_TOGETHER_SUITE);
        })
        .then(() => {
          // SMART_LOCK always requires authentication.
          return enableFeatureWithAuthFn(Feature.SMART_LOCK);
        });
  });

  test('Nearby setup button visibility', async () => {
    assertTrue(test_util.isChildVisible(
        multidevicePage, '#nearbySetUp', /*checkLightDom=*/ false));
    assertFalse(test_util.isChildVisible(
        multidevicePage, '#nearbySharingToggleButton',
        /*checkLightDom=*/ false));

    multidevicePage.setPrefValue('nearby_sharing.onboarding_complete', true);
    Polymer.dom.flush();

    assertFalse(test_util.isChildVisible(
        multidevicePage, '#nearbySetUp', /*checkLightDom=*/ false));
    assertTrue(test_util.isChildVisible(
        multidevicePage, '#nearbySharingToggleButton',
        /*checkLightDom=*/ false));
  });

  test('Nearby description shown before onboarding is completed', async () => {
    assertTrue(test_util.isChildVisible(
        multidevicePage, '#nearbyShareSecondary > settings-localized-link',
        /*checkLightDom=*/ false));

    multidevicePage.setPrefValue('nearby_sharing.onboarding_complete', true);
    Polymer.dom.flush();

    assertFalse(test_util.isChildVisible(
        multidevicePage, '#nearbyShareSecondary > settings-localized-link',
        /*checkLightDom=*/ false));

    assertEquals(
        multidevicePage.$$('#nearbyShareSecondary').textContent.trim(), 'Off');
  });

  test('Better Together Suite icon visible when there is no host set', () => {
    setHostData(settings.MultiDeviceSettingsMode.NO_HOST_SET);
    assertTrue(test_util.isChildVisible(
        multidevicePage, '#betterTogetherSuiteIcon', /*checkLightDom=*/ false));
  });

  test('Better Together Suite icon visible when there is a host set', () => {
    setHostData(settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    assertTrue(test_util.isChildVisible(
        multidevicePage, '#betterTogetherSuiteIcon', /*checkLightDom=*/ false));
  });

  test('Better Together Suite icon remains visible when host added', () => {
    setHostData(settings.MultiDeviceSettingsMode.NO_HOST_SET);
    assertTrue(test_util.isChildVisible(
        multidevicePage, '#betterTogetherSuiteIcon', /*checkLightDom=*/ false));

    setHostData(settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    assertTrue(test_util.isChildVisible(
        multidevicePage, '#betterTogetherSuiteIcon', /*checkLightDom=*/ false));
  });

  test('Better Together Suite icon remains visible when host removed', () => {
    setHostData(settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    assertTrue(test_util.isChildVisible(
        multidevicePage, '#betterTogetherSuiteIcon', /*checkLightDom=*/ false));

    setHostData(settings.MultiDeviceSettingsMode.NO_HOST_SET);
    assertTrue(test_util.isChildVisible(
        multidevicePage, '#betterTogetherSuiteIcon', /*checkLightDom=*/ false));
  });
});
