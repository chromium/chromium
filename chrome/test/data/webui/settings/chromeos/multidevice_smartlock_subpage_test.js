// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MultiDeviceBrowserProxyImpl, MultiDeviceFeatureState, Router, routes, SmartLockSignInEnabledState} from 'chrome://os-settings/chromeos/os_settings.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {eventToPromise, waitAfterNextRender} from 'chrome://test/test_util.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

import {TestMultideviceBrowserProxy} from './test_multidevice_browser_proxy.js';

suite('Multidevice', function() {
  let smartLockSubPage = null;
  let browserProxy = null;

  function createSmartLockSubPage() {
    const smartLockSubPage =
        document.createElement('settings-multidevice-smartlock-subpage');
    document.body.appendChild(smartLockSubPage);
    flush();

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

    browserProxy = new TestMultideviceBrowserProxy();
    MultiDeviceBrowserProxyImpl.instance_ = browserProxy;

    // Each test must create a settings-multidevice-smartlock-subpage element.
    smartLockSubPage = null;
  });

  teardown(function() {
    assertTrue(!!smartLockSubPage);
    smartLockSubPage.remove();

    browserProxy.reset();
    Router.getInstance().resetRouteForTesting();
  });

  test('Smart Lock enabled', function() {
    smartLockSubPage = createSmartLockSubPage();
    setSmartLockFeatureState(MultiDeviceFeatureState.ENABLED_BY_USER);

    // Feature toggle is checked.
    const toggleControl = getSmartLockFeatureToggleControl();
    assertTrue(toggleControl.checked);

    // Screen lock options are visible.
    const optionsContent = getScreenLockOptionsContent();
    assertTrue(optionsContent.opened);
  });

  test('Smart Lock disabled by user', function() {
    smartLockSubPage = createSmartLockSubPage();
    setSmartLockFeatureState(MultiDeviceFeatureState.DISABLED_BY_USER);

    // Feature toggle is not checked.
    const toggleControl = getSmartLockFeatureToggleControl();
    assertFalse(toggleControl.checked);

    // Screen lock options are not visible.
    const optionsContent = getScreenLockOptionsContent();
    assertFalse(optionsContent.opened);
  });

  test('Smart Lock prohibited by policy', function() {
    smartLockSubPage = createSmartLockSubPage();
    setSmartLockFeatureState(MultiDeviceFeatureState.PROHIBITED_BY_POLICY);

    // Feature toggle is not checked.
    const toggleControl = getSmartLockFeatureToggleControl();
    assertFalse(toggleControl.checked);

    // Screen lock options are not visible.
    const optionsContent = getScreenLockOptionsContent();
    assertFalse(optionsContent.opened);
  });

  test('Smart Lock enable feature toggle', function() {
    smartLockSubPage = createSmartLockSubPage();
    setSuiteState(MultiDeviceFeatureState.ENABLED_BY_USER);
    setSmartLockFeatureState(MultiDeviceFeatureState.DISABLED_BY_USER);

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
        eventToPromise('feature-toggle-clicked', featureToggle).then(() => {
          setSmartLockFeatureState(MultiDeviceFeatureState.ENABLED_BY_USER);
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
    setSuiteState(MultiDeviceFeatureState.ENABLED_BY_USER);
    setSmartLockFeatureState(MultiDeviceFeatureState.DISABLED_BY_USER);

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
        eventToPromise('feature-toggle-clicked', featureToggle)
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

  test('Deep link to smart lock on/off', async () => {
    smartLockSubPage = createSmartLockSubPage();
    setSuiteState(MultiDeviceFeatureState.ENABLED_BY_USER);
    setSmartLockFeatureState(MultiDeviceFeatureState.DISABLED_BY_USER);

    const params = new URLSearchParams();
    params.append('settingId', '203');
    Router.getInstance().navigateTo(routes.SMART_LOCK, params);

    flush();

    const deepLinkElement = getSmartLockFeatureToggleControl();
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Smart lock on/off toggle should be focused for settingId=203.');
  });

  test('Smart Lock signin disabled by default', function() {
    smartLockSubPage = createSmartLockSubPage();

    const smartLockSignInRadio = getSmartLockSignInRadio();
    assertEquals(
        SmartLockSignInEnabledState.DISABLED, smartLockSignInRadio.selected);
  });

  test('Smart Lock signin enabled', function() {
    const whenSignInEnabledSet =
        browserProxy.whenCalled('getSmartLockSignInEnabled');

    smartLockSubPage = createSmartLockSubPage();

    const smartLockSignInRadio = getSmartLockSignInRadio();
    assertEquals(
        SmartLockSignInEnabledState.DISABLED, smartLockSignInRadio.selected);

    return whenSignInEnabledSet.then(() => {
      assertEquals(
          SmartLockSignInEnabledState.ENABLED, smartLockSignInRadio.selected);
    });
  });

  test('Smart Lock signin enabled changed', function() {
    smartLockSubPage = createSmartLockSubPage();

    const smartLockSignInRadio = getSmartLockSignInRadio();
    assertEquals(
        SmartLockSignInEnabledState.DISABLED, smartLockSignInRadio.selected);

    webUIListenerCallback('smart-lock-signin-enabled-changed', true);
    flush();

    assertEquals(
        SmartLockSignInEnabledState.ENABLED, smartLockSignInRadio.selected);
  });

  test('Smart Lock sign in successful authentication', function() {
    smartLockSubPage = createSmartLockSubPage();

    const smartLockSignInRadio = getSmartLockSignInRadio();
    assertEquals(
        SmartLockSignInEnabledState.DISABLED, smartLockSignInRadio.selected);

    // The password dialog is not visible.
    let passwordDialog = smartLockSubPage.$$('settings-password-prompt-dialog');
    assertTrue(!passwordDialog);

    // Click the 'Enable sign in' radio.
    const enableSmartLockControl =
        smartLockSubPage.$$('multidevice-radio-button[name="enabled"]');
    assertTrue(!!enableSmartLockControl);
    enableSmartLockControl.click();
    flush();

    // The password dialog is now visible.
    passwordDialog = smartLockSubPage.$$('settings-password-prompt-dialog');
    assertTrue(!!passwordDialog);

    // Sign in radio is still disabled because the user has not authenticated
    // using the password dialog.
    assertEquals(
        SmartLockSignInEnabledState.DISABLED, smartLockSignInRadio.selected);

    // Simulate the user entering a valid password into the dialog.
    passwordDialog.authToken = 'validAuthToken';
    passwordDialog.dispatchEvent(new CustomEvent('close'));
    flush();

    return browserProxy.whenCalled('getSmartLockSignInEnabled').then(params => {
      assertEquals(
          SmartLockSignInEnabledState.ENABLED, smartLockSignInRadio.selected);
    });
  });

  test('Smart Lock sign in cancel authentication', function() {
    smartLockSubPage = createSmartLockSubPage();

    const smartLockSignInRadio = getSmartLockSignInRadio();
    assertEquals(
        SmartLockSignInEnabledState.DISABLED, smartLockSignInRadio.selected);

    // The password dialog is not visible.
    let passwordDialog = smartLockSubPage.$$('settings-password-prompt-dialog');
    assertTrue(!passwordDialog);

    // Click the 'Enable sign in' radio.
    const enableSmartLockControl =
        smartLockSubPage.$$('multidevice-radio-button[name="enabled"]');
    assertTrue(!!enableSmartLockControl);
    enableSmartLockControl.click();
    flush();

    // The password dialog is now visible.
    passwordDialog = smartLockSubPage.$$('settings-password-prompt-dialog');
    assertTrue(!!passwordDialog);

    // Sign in radio is still disabled because the user has not authenticated
    // using the password dialog.
    assertEquals(
        SmartLockSignInEnabledState.DISABLED, smartLockSignInRadio.selected);

    // Simulate the user closing the password dialog.
    passwordDialog.dispatchEvent(new CustomEvent('close'));
    flush();

    // Verify the password dialog is closed.
    passwordDialog = smartLockSubPage.$$('settings-password-prompt-dialog');
    assert(!passwordDialog);

    // The password dialog is closed and unauthenticated, so sign in is still
    // disabled.
    assertEquals(
        SmartLockSignInEnabledState.DISABLED, smartLockSignInRadio.selected);
  });

  test('Smart Lock disable sign in does not show password dialog', function() {
    smartLockSubPage = createSmartLockSubPage();

    // Set sign in as enabled.
    webUIListenerCallback('smart-lock-signin-enabled-changed', true);
    flush();

    const smartLockSignInRadio = getSmartLockSignInRadio();
    assertEquals(
        SmartLockSignInEnabledState.ENABLED, smartLockSignInRadio.selected);

    // The password dialog is not visible.
    let passwordDialog = smartLockSubPage.$$('settings-password-prompt-dialog');
    assertTrue(!passwordDialog);

    // Click the 'Disable sign in' radio.
    const disableSmartLockControl =
        smartLockSubPage.$$('multidevice-radio-button[name="disabled"]');
    assertTrue(!!disableSmartLockControl);
    disableSmartLockControl.click();
    flush();

    // The password dialog is not shown.
    passwordDialog = smartLockSubPage.$$('settings-password-prompt-dialog');
    assertTrue(!passwordDialog);

    // Sign in radio is now disabled.
    assertEquals(
        SmartLockSignInEnabledState.DISABLED, smartLockSignInRadio.selected);
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

    webUIListenerCallback('smart-lock-signin-allowed-changed', false);
    flush();

    assertTrue(smartLockSignInRadio.disabled);
  });
});
