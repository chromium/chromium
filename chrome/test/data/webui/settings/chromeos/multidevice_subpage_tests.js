// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('Multidevice', function() {
  let multideviceSubpage = null;
  let browserProxy = null;
  // Although HOST_SET_MODES is effectively a constant, it cannot reference the
  // enum settings.MultiDeviceSettingsMode from here so its initialization is
  // deferred to the suiteSetup function.
  let HOST_SET_MODES;

  /**
   * Observably sets mode. Everything else remains unchanged.
   * @param {settings.MultiDeviceSettingsMode} newMode
   */
  function setMode(newMode) {
    multideviceSubpage.pageContentData =
        Object.assign({}, multideviceSubpage.pageContentData, {
          mode: newMode,
        });
    Polymer.dom.flush();
  }

  /**
   * Observably resets feature states so that each feature is supported if and
   * only if it is in the provided array. Everything else remains unchanged.
   * @param {Array<settings.MultiDeviceFeature>} supportedFeatures
   */
  function setSupportedFeatures(supportedFeatures) {
    multideviceSubpage.pageContentData =
        Object.assign({}, multideviceSubpage.pageContentData, {
          betterTogetherState:
              supportedFeatures.includes(
                  settings.MultiDeviceFeature.BETTER_TOGETHER_SUITE) ?
              settings.MultiDeviceFeatureState.ENABLED_BY_USER :
              settings.MultiDeviceFeatureState.NOT_SUPPORTED_BY_CHROMEBOOK,
          instantTetheringState:
              supportedFeatures.includes(
                  settings.MultiDeviceFeature.INSTANT_TETHERING) ?
              settings.MultiDeviceFeatureState.ENABLED_BY_USER :
              settings.MultiDeviceFeatureState.NOT_SUPPORTED_BY_CHROMEBOOK,
          messagesState:
              supportedFeatures.includes(settings.MultiDeviceFeature.MESSAGES) ?
              settings.MultiDeviceFeatureState.ENABLED_BY_USER :
              settings.MultiDeviceFeatureState.NOT_SUPPORTED_BY_CHROMEBOOK,
          smartLockState: supportedFeatures.includes(
                              settings.MultiDeviceFeature.SMART_LOCK) ?
              settings.MultiDeviceFeatureState.ENABLED_BY_USER :
              settings.MultiDeviceFeatureState.NOT_SUPPORTED_BY_CHROMEBOOK,
        });
    Polymer.dom.flush();
  }

  /**
   * @param {boolean} pairingComplete
   */
  function setAndroidSmsPairingComplete(pairingComplete) {
    multideviceSubpage.pageContentData =
        Object.assign({}, multideviceSubpage.pageContentData, {
          messagesState: pairingComplete ?
              settings.MultiDeviceFeatureState.ENABLED_BY_USER :
              settings.MultiDeviceFeatureState.FURTHER_SETUP_REQUIRED,
          isAndroidSmsPairingComplete: pairingComplete,
        });
    Polymer.dom.flush();
  }

  suiteSetup(function() {
    HOST_SET_MODES = [
      settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER,
      settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
      settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED,
    ];
  });

  setup(function() {
    browserProxy = new multidevice.TestMultideviceBrowserProxy();
    settings.MultiDeviceBrowserProxyImpl.instance_ = browserProxy;

    PolymerTest.clearBody();
    multideviceSubpage = document.createElement('settings-multidevice-subpage');
    multideviceSubpage.pageContentData = {hostDeviceName: 'Pixel XL'};
    setMode(settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    setSupportedFeatures(Object.values(settings.MultiDeviceFeature));

    document.body.appendChild(multideviceSubpage);
    Polymer.dom.flush();
  });

  teardown(function() {
    multideviceSubpage.remove();
  });

  test('individual features appear only if host is verified', function() {
    for (const mode of HOST_SET_MODES) {
      setMode(mode);
      assertEquals(
          !!multideviceSubpage.$$('#smartLockItem'),
          mode == settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED);
      assertEquals(
          !!multideviceSubpage.$$('#instantTetheringItem'),
          mode == settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED);
      assertEquals(
          !!multideviceSubpage.$$('#messagesItem'),
          mode == settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    }
  });

  test(
      'individual features are attached only if they are supported',
      function() {
        assertTrue(!!multideviceSubpage.$$('#smartLockItem'));
        assertTrue(!!multideviceSubpage.$$('#instantTetheringItem'));
        assertTrue(!!multideviceSubpage.$$('#messagesItem'));

        setSupportedFeatures([
          settings.MultiDeviceFeature.SMART_LOCK,
          settings.MultiDeviceFeature.MESSAGES,
        ]);
        assertTrue(!!multideviceSubpage.$$('#smartLockItem'));
        assertFalse(!!multideviceSubpage.$$('#instantTetheringItem'));
        assertTrue(!!multideviceSubpage.$$('#messagesItem'));

        setSupportedFeatures([settings.MultiDeviceFeature.INSTANT_TETHERING]);
        assertFalse(!!multideviceSubpage.$$('#smartLockItem'));
        assertTrue(!!multideviceSubpage.$$('#instantTetheringItem'));
        assertFalse(!!multideviceSubpage.$$('#messagesItem'));

        setSupportedFeatures([]);
        assertFalse(!!multideviceSubpage.$$('#smartLockItem'));
        assertFalse(!!multideviceSubpage.$$('#instantTetheringItem'));
        assertFalse(!!multideviceSubpage.$$('#messagesItem'));
      });

  test('clicking SmartLock item routes to SmartLock subpage', function() {
    multideviceSubpage.$$('#smartLockItem').$$('.link-wrapper').click();
    assertEquals(settings.getCurrentRoute(), settings.routes.SMART_LOCK);
  });

  test('AndroidMessages item shows button when not set up', function() {
    setAndroidSmsPairingComplete(false);
    Polymer.dom.flush();

    const controllerSelector = '#messagesItem > [slot=feature-controller]';
    assertTrue(!!multideviceSubpage.$$(controllerSelector));
    assertTrue(
        multideviceSubpage.$$(controllerSelector).tagName.includes('BUTTON'));

    setAndroidSmsPairingComplete(true);
    Polymer.dom.flush();

    assertFalse(!!multideviceSubpage.$$(controllerSelector));
  });

  test(
      'AndroidMessages set up button calls browser proxy function', function() {
        setAndroidSmsPairingComplete(false);
        Polymer.dom.flush();

        const setUpButton =
            multideviceSubpage.$$('#messagesItem > [slot=feature-controller]');
        assertTrue(!!setUpButton);

        setUpButton.click();

        return browserProxy.whenCalled('setUpAndroidSms');
      });

  test(
      'AndroidMessages toggle is disabled when prohibited by policy',
      function() {
        // Verify that setup button is disabled when prohibited by policy.
        multideviceSubpage.pageContentData =
            Object.assign({}, multideviceSubpage.pageContentData, {
              messagesState:
                  settings.MultiDeviceFeatureState.PROHIBITED_BY_POLICY,
              isAndroidSmsPairingComplete: false,
            });
        Polymer.dom.flush();

        let setUpButton =
            multideviceSubpage.$$('#messagesItem > [slot=feature-controller]');
        assertFalse(!!setUpButton);

        // Verify that setup button is not disabled when feature is enabled.
        setAndroidSmsPairingComplete(false);
        setUpButton =
            multideviceSubpage.$$('#messagesItem > [slot=feature-controller]');
        assertTrue(!!setUpButton);
        assertTrue(setUpButton.tagName.includes('BUTTON'));
        assertFalse(setUpButton.disabled);
      });
});
