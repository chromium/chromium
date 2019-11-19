// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  function setSuiteState(newState) {
    setPageContentData(Object.assign(
        {}, multidevicePage.pageContentData, {betterTogetherState: newState}));
  }

  function setSmartLockState(newState) {
    setPageContentData(Object.assign(
        {}, multidevicePage.pageContentData, {smartLockState: newState}));
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
      multidevicePage.$$('#multidevicePasswordPrompt').authToken =
          'validAuthToken';
      // Simulate closing the password prompt dialog
      multidevicePage.$$('#multidevicePasswordPrompt').fire('close');
      Polymer.dom.flush();
    } else {
      assertFalse(multidevicePage.showPasswordPromptDialog_);
    }

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

    multidevicePage = document.createElement('settings-multidevice-page');
    assertTrue(!!multidevicePage);

    document.body.appendChild(multidevicePage);
    return browserProxy.whenCalled('getPageContentData');
  });

  teardown(function() {
    multidevicePage.remove();
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
});
