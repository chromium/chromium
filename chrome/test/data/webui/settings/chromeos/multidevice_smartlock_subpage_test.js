// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('Multidevice', function() {
  let smartLockSubPage = null;
  let browserProxy = null;

  function createSmartLockSubPage() {
    const smartLockSubPage =
        document.createElement('settings-multidevice-smartlock-subpage');
    document.body.appendChild(smartLockSubPage);
    Polymer.dom.flush();

    return smartLockSubPage;
  }

  function setSuiteState(newState) {
    smartLockSubPage.pageContentData = Object.assign(
        {}, smartLockSubPage.pageContentData, {betterTogetherState: newState});
  }

  function setSmartLockFeatureState(newState) {
    smartLockSubPage.pageContentData = Object.assign(
        {}, smartLockSubPage.pageContentData, {smartLockState: newState});
  }

  function getSmartLockFeatureToggle() {
    const smartLockFeatureToggle =
        smartLockSubPage.$$('settings-multidevice-feature-toggle');
    assertTrue(!!smartLockFeatureToggle);
    return smartLockFeatureToggle;
  }

  function getSmartLockFeatureToggleControl() {
    const smartLockFeatureToggle = getSmartLockFeatureToggle();
    const toggleControl = smartLockFeatureToggle.$$('#toggle');
    assertTrue(!!toggleControl);
    return toggleControl;
  }

  function getScreenLockOptionsContent() {
    const optionsContent = smartLockSubPage.$$('iron-collapse');
    assertTrue(!!optionsContent);
    return optionsContent;
  }

  function getSmartLockSignInRadio() {
    const smartLockSignInRadio = smartLockSubPage.$$('cr-radio-group');
    assertTrue(!!smartLockSignInRadio);
    return smartLockSignInRadio;
  }

  setup(function() {
    PolymerTest.clearBody();

    browserProxy = new multidevice.TestMultideviceBrowserProxy();
    settings.MultiDeviceBrowserProxyImpl.instance_ = browserProxy;

    // Each test must create a settings-multidevice-smartlock-subpage element.
    smartLockSubPage = null;
  });

  teardown(function() {
    assertTrue(!!smartLockSubPage);
    smartLockSubPage.remove();

    browserProxy.reset();
  });

  test('Smart Lock enabled', function() {
    smartLockSubPage = createSmartLockSubPage();
    setSmartLockFeatureState(settings.MultiDeviceFeatureState.ENABLED_BY_USER);

    // Feature toggle is checked.
    const toggleControl = getSmartLockFeatureToggleControl();
    assertTrue(toggleControl.checked);

    // Screen lock options are visible.
    const optionsContent = getScreenLockOptionsContent();
    assertTrue(optionsContent.opened);
  });

  test('Smart Lock disabled by user', function() {
    smartLockSubPage = createSmartLockSubPage();
    setSmartLockFeatureState(settings.MultiDeviceFeatureState.DISABLED_BY_USER);

    // Feature toggle is not checked.
    const toggleControl = getSmartLockFeatureToggleControl();
    assertFalse(toggleControl.checked);

    // Screen lock options are not visible.
    const optionsContent = getScreenLockOptionsContent();
    assertFalse(optionsContent.opened);
  });

  test('Smart Lock prohibited by policy', function() {
    smartLockSubPage = createSmartLockSubPage();
    setSmartLockFeatureState(
        settings.MultiDeviceFeatureState.PROHIBITED_BY_POLICY);

    // Feature toggle is not checked.
    const toggleControl = getSmartLockFeatureToggleControl();
    assertFalse(toggleControl.checked);

    // Screen lock options are not visible.
    const optionsContent = getScreenLockOptionsContent();
    assertFalse(optionsContent.opened);
  });

  test('Smart Lock enable feature toggle', function() {
    smartLockSubPage = createSmartLockSubPage();
    setSuiteState(settings.MultiDeviceFeatureState.ENABLED_BY_USER);
    setSmartLockFeatureState(settings.MultiDeviceFeatureState.DISABLED_BY_USER);

    // Feature toggle is not checked.
    const toggleControl = getSmartLockFeatureToggleControl();
    assertFalse(toggleControl.checked);

    // In the case of Smart Lock, the multidevice-feature-toggle depends on the
    // top-level settings-multidevice-page handling the authentication process
    // and feature toggling.
    //
    // This code simulates the authentication and toggling by directly toggling
    // the feature to enabled when the feature toggle is clicked.
    const featureToggle = getSmartLockFeatureToggle();
    const whenFeatureClicked =
        test_util.eventToPromise('feature-toggle-clicked', featureToggle)
            .then(() => {
              setSmartLockFeatureState(
                  settings.MultiDeviceFeatureState.ENABLED_BY_USER);
            });

    // Toggle the feature to enabled.
    toggleControl.click();

    // Verify the feature control is toggled to enabled after the user clicks on
    // the feature toggle.
    return whenFeatureClicked.then(() => {
      assertTrue(toggleControl.checked);
    });
  });

  test('Smart Lock enable feature toggle without authentication', function() {
    smartLockSubPage = createSmartLockSubPage();
    setSuiteState(settings.MultiDeviceFeatureState.ENABLED_BY_USER);
    setSmartLockFeatureState(settings.MultiDeviceFeatureState.DISABLED_BY_USER);

    // Feature toggle is not checked.
    const toggleControl = getSmartLockFeatureToggleControl();
    assertFalse(toggleControl.checked);

    // In the case of Smart Lock, the multidevice-feature-toggle depends on the
    // top-level settings-multidevice-page handling the authentication process
    // and feature toggling.
    //
    // This code simulates the user cancelling authentication by ignoring the
    // toggle being clicked.
    const featureToggle = getSmartLockFeatureToggle();
    const whenFeatureClicked =
        test_util.eventToPromise('feature-toggle-clicked', featureToggle)
            .then(
                () => {
                    // Do not enable the feature (simulating the user cancelling
                    // auth).
                });

    // Toggle the feature to enabled.
    toggleControl.click();

    // Verify the feature control is not toggled to enabled after the user
    // cancels authenticating with the password dialog.
    return whenFeatureClicked.then(() => {
      assertFalse(toggleControl.checked);
    });
  });

  test('Smart Lock signin disabled by default', function() {
    smartLockSubPage = createSmartLockSubPage();

    const smartLockSignInRadio = getSmartLockSignInRadio();
    assertEquals(
        settings.SignInEnabledState.DISABLED, smartLockSignInRadio.selected);
  });

  test('Smart Lock signin enabled', function() {
    const whenSignInEnabledSet =
        browserProxy.whenCalled('getSmartLockSignInEnabled');

    smartLockSubPage = createSmartLockSubPage();

    const smartLockSignInRadio = getSmartLockSignInRadio();
    assertEquals(
        settings.SignInEnabledState.DISABLED, smartLockSignInRadio.selected);

    return whenSignInEnabledSet.then(() => {
      assertEquals(
          settings.SignInEnabledState.ENABLED, smartLockSignInRadio.selected);
    });
  });

  test('Smart Lock signin enabled changed', function() {
    smartLockSubPage = createSmartLockSubPage();

    const smartLockSignInRadio = getSmartLockSignInRadio();
    assertEquals(
        settings.SignInEnabledState.DISABLED, smartLockSignInRadio.selected);

    cr.webUIListenerCallback('smart-lock-signin-enabled-changed', true);
    Polymer.dom.flush();

    assertEquals(
        settings.SignInEnabledState.ENABLED, smartLockSignInRadio.selected);
  });

  test('Smart Lock sign in successful authentication', function() {
    smartLockSubPage = createSmartLockSubPage();

    const smartLockSignInRadio = getSmartLockSignInRadio();
    assertEquals(
        settings.SignInEnabledState.DISABLED, smartLockSignInRadio.selected);

    // The password dialog is not visible.
    let passwordDialog = smartLockSubPage.$$('settings-password-prompt-dialog');
    assertTrue(!passwordDialog);

    // Click the 'Enable sign in' radio.
    const enableSmartLockControl =
        smartLockSubPage.$$('multidevice-radio-button[name="enabled"]');
    assertTrue(!!enableSmartLockControl);
    enableSmartLockControl.click();
    Polymer.dom.flush();

    // The password dialog is now visible.
    passwordDialog = smartLockSubPage.$$('settings-password-prompt-dialog');
    assertTrue(!!passwordDialog);

    // Sign in radio is still disabled because the user has not authenticated
    // using the password dialog.
    assertEquals(
        settings.SignInEnabledState.DISABLED, smartLockSignInRadio.selected);

    // Simulate the user entering a valid password into the dialog.
    passwordDialog.authToken = 'validAuthToken';
    passwordDialog.dispatchEvent(new CustomEvent('close'));
    Polymer.dom.flush();

    return browserProxy.whenCalled('getSmartLockSignInEnabled').then(params => {
      assertEquals(
          settings.SignInEnabledState.ENABLED, smartLockSignInRadio.selected);
    });
  });

  test('Smart Lock sign in cancel authentication', function() {
    smartLockSubPage = createSmartLockSubPage();

    const smartLockSignInRadio = getSmartLockSignInRadio();
    assertEquals(
        settings.SignInEnabledState.DISABLED, smartLockSignInRadio.selected);

    // The password dialog is not visible.
    let passwordDialog = smartLockSubPage.$$('settings-password-prompt-dialog');
    assertTrue(!passwordDialog);

    // Click the 'Enable sign in' radio.
    const enableSmartLockControl =
        smartLockSubPage.$$('multidevice-radio-button[name="enabled"]');
    assertTrue(!!enableSmartLockControl);
    enableSmartLockControl.click();
    Polymer.dom.flush();

    // The password dialog is now visible.
    passwordDialog = smartLockSubPage.$$('settings-password-prompt-dialog');
    assertTrue(!!passwordDialog);

    // Sign in radio is still disabled because the user has not authenticated
    // using the password dialog.
    assertEquals(
        settings.SignInEnabledState.DISABLED, smartLockSignInRadio.selected);

    // Simulate the user closing the password dialog.
    passwordDialog.dispatchEvent(new CustomEvent('close'));
    Polymer.dom.flush();

    // Verify the password dialog is closed.
    passwordDialog = smartLockSubPage.$$('settings-password-prompt-dialog');
    assert(!passwordDialog);

    // The password dialog is closed and unauthenticated, so sign in is still
    // disabled.
    assertEquals(
        settings.SignInEnabledState.DISABLED, smartLockSignInRadio.selected);
  });

  test('Smart Lock disable sign in does not show password dialog', function() {
    smartLockSubPage = createSmartLockSubPage();

    // Set sign in as enabled.
    cr.webUIListenerCallback('smart-lock-signin-enabled-changed', true);
    Polymer.dom.flush();

    const smartLockSignInRadio = getSmartLockSignInRadio();
    assertEquals(
        settings.SignInEnabledState.ENABLED, smartLockSignInRadio.selected);

    // The password dialog is not visible.
    let passwordDialog = smartLockSubPage.$$('settings-password-prompt-dialog');
    assertTrue(!passwordDialog);

    // Click the 'Disable sign in' radio.
    const disableSmartLockControl =
        smartLockSubPage.$$('multidevice-radio-button[name="disabled"]');
    assertTrue(!!disableSmartLockControl);
    disableSmartLockControl.click();
    Polymer.dom.flush();

    // The password dialog is not shown.
    passwordDialog = smartLockSubPage.$$('settings-password-prompt-dialog');
    assertTrue(!passwordDialog);

    // Sign in radio is now disabled.
    assertEquals(
        settings.SignInEnabledState.DISABLED, smartLockSignInRadio.selected);
  });

  test('Smart Lock sign in control enabled by default', function() {
    smartLockSubPage = createSmartLockSubPage();

    const smartLockSignInRadio = getSmartLockSignInRadio();
    assertFalse(smartLockSignInRadio.disabled);
  });

  test('Smart Lock sign in control disabled by policy', function() {
    browserProxy.smartLockSignInAllowed = false;
    const whenSignInAllowedSet =
        browserProxy.whenCalled('getSmartLockSignInAllowed');

    smartLockSubPage = createSmartLockSubPage();

    return whenSignInAllowedSet.then(() => {
      const smartLockSignInRadio = getSmartLockSignInRadio();
      assertTrue(smartLockSignInRadio.disabled);
    });
  });

  test('Smart Lock sign in control allowed state changes', function() {
    smartLockSubPage = createSmartLockSubPage();

    const smartLockSignInRadio = getSmartLockSignInRadio();
    assertFalse(smartLockSignInRadio.disabled);

    cr.webUIListenerCallback('smart-lock-signin-allowed-changed', false);
    Polymer.dom.flush();

    assertTrue(smartLockSignInRadio.disabled);
  });
});
