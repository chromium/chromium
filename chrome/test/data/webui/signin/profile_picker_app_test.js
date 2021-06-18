// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LocalProfileCustomizationElement, ProfileTypeChoiceElement} from 'chrome://profile-picker/lazy_load.js';
import {ensureLazyLoaded, ManageProfilesBrowserProxyImpl, navigateTo, ProfilePickerAppElement, ProfilePickerMainViewElement, Routes} from 'chrome://profile-picker/profile_picker.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertTrue} from '../chai_assert.js';
import {flushTasks, waitBeforeNextRender, whenAttributeIs, whenCheck} from '../test_util.m.js';

import {TestManageProfilesBrowserProxy} from './test_manage_profiles_browser_proxy.js';

suite('ProfilePickerAppTest', function() {
  /** @type {!ProfilePickerAppElement} */
  let testElement;

  /** @type {!TestManageProfilesBrowserProxy} */
  let browserProxy;

  function resetTestElement(route) {
    document.body.innerHTML = '';
    navigateTo(route);
    testElement = /** @type {!ProfilePickerAppElement} */ (
        document.createElement('profile-picker-app'));
    document.body.appendChild(testElement);
    return waitBeforeNextRender(testElement);
  }

  setup(function() {
    browserProxy = new TestManageProfilesBrowserProxy();
    ManageProfilesBrowserProxyImpl.instance_ = browserProxy;

    return resetTestElement(Routes.MAIN);
  });

  /**
   * @return {!Promise} Promise that resolves when initialization is complete
   *     and the lazy loaded module has been loaded.
   */
  async function waitForProfileCretionLoad() {
    await Promise.all([
      browserProxy.whenCalled('getNewProfileSuggestedThemeInfo'),
      ensureLazyLoaded(),
    ]);
    browserProxy.reset();
  }

  /** @param {!HTMLElement} element */
  function verifyProfileCreationViewStyle(element) {
    assertEquals(
        getComputedStyle(element.shadowRoot.querySelector('#headerContainer'))
            .getPropertyValue('--theme-frame-color')
            .trim(),
        browserProxy.profileThemeInfo.themeFrameColor);
    assertEquals(
        getComputedStyle(element.shadowRoot.querySelector('#headerContainer'))
            .getPropertyValue('--theme-text-color')
            .trim(),
        browserProxy.profileThemeInfo.themeFrameTextColor);
    assertEquals(
        getComputedStyle(element.shadowRoot.querySelector('#headerContainer'))
            .backgroundColor,
        browserProxy.profileThemeInfo.themeFrameColor);
    assertEquals(
        getComputedStyle(element.shadowRoot.querySelector('#backButton'))
            .getPropertyValue('--cr-icon-button-fill-color')
            .trim(),
        browserProxy.profileThemeInfo.themeFrameTextColor);
  }

  test('ProfilePickerMainView', async function() {
    assertEquals(
        testElement.shadowRoot.querySelectorAll('[slot=view]').length, 1);
    const mainView = /** @type {!ProfilePickerMainViewElement} */ (
        testElement.shadowRoot.querySelector('profile-picker-main-view'));
    await whenCheck(mainView, () => mainView.classList.contains('active'));
    await browserProxy.whenCalled('initializeMainView');
    assertTrue(mainView.shadowRoot.querySelector('#wrapper').hidden);

    webUIListenerCallback(
        'profiles-list-changed', [browserProxy.profileSample]);
    flushTasks();
    assertEquals(
        mainView.shadowRoot.querySelector('#wrapper')
            .querySelectorAll('profile-card')
            .length,
        1);
    mainView.shadowRoot.querySelector('#addProfile').click();
    await waitForProfileCretionLoad();
    assertEquals(
        testElement.shadowRoot.querySelectorAll('[slot=view]').length, 2);
    const choice = /** @type {!ProfileTypeChoiceElement} */ (
        testElement.shadowRoot.querySelector('profile-type-choice'));
    assertTrue(!!choice);
    await whenCheck(choice, () => choice.classList.contains('active'));
    verifyProfileCreationViewStyle(choice);
  });

  test('SignInPromoSignIn', async function() {
    await resetTestElement(Routes.NEW_PROFILE);
    await waitForProfileCretionLoad();
    const choice = /** @type {!ProfileTypeChoiceElement} */ (
        testElement.shadowRoot.querySelector('profile-type-choice'));
    assertTrue(!!choice);
    choice.shadowRoot.querySelector('#signInButton').click();
    assertTrue(choice.shadowRoot.querySelector('#signInButton').disabled);
    assertTrue(choice.shadowRoot.querySelector('#notNowButton').disabled);
    assertTrue(choice.shadowRoot.querySelector('#backButton').disabled);
    return browserProxy.whenCalled('loadSignInProfileCreationFlow');
  });

  test('ThemeColorConsistentInProfileCreationViews', async function() {
    await resetTestElement(Routes.NEW_PROFILE);
    await waitForProfileCretionLoad();
    const choice = /** @type {!ProfileTypeChoiceElement} */ (
        testElement.shadowRoot.querySelector('profile-type-choice'));
    assertTrue(!!choice);
    await whenCheck(choice, () => choice.classList.contains('active'));
    verifyProfileCreationViewStyle(choice);
    choice.shadowRoot.querySelector('#notNowButton').click();
    await waitBeforeNextRender(testElement);
    const customization =
        /** @type {!LocalProfileCustomizationElement} */ (
            testElement.shadowRoot.querySelector(
                'local-profile-customization'));
    assertTrue(!!customization);
    await whenCheck(
        customization, () => customization.classList.contains('active'));
    verifyProfileCreationViewStyle(customization);

    // Test color changes from the local profile customization is reflected in
    // the profile type choice.
    browserProxy.resetResolver('getProfileThemeInfo');
    const colorPicker = customization.shadowRoot.querySelector('#colorPicker');
    assertTrue(!!colorPicker);
    assertTrue(!!colorPicker.selectedTheme);
    browserProxy.setProfileThemeInfo({
      color: -3413569,
      colorId: 7,
      themeFrameColor: 'rgb(203, 233, 191)',
      themeFrameTextColor: 'rgb(32, 33, 36)',
      themeGenericAvatar: 'AvatarUrl-7',
      themeShapeColor: 'rgb(255, 255, 255)'
    });
    // Select different color.
    colorPicker.selectedTheme = {
      type: 2,
      info: {
        chromeThemeId: browserProxy.profileThemeInfo.colorId,
      },
    };
    await browserProxy.whenCalled('getProfileThemeInfo');
    verifyProfileCreationViewStyle(customization);
    customization.shadowRoot.querySelector('#backButton').click();
    await whenCheck(choice, () => choice.classList.contains('active'));
    verifyProfileCreationViewStyle(choice);
  });

  test('ProfileCreationNotAllowed', async function() {
    loadTimeData.overrideValues({
      isProfileCreationAllowed: false,
    });
    await resetTestElement(Routes.NEW_PROFILE);
    assertEquals(
        testElement.shadowRoot.querySelectorAll('[slot=view]').length, 1);
    const mainView = /** @type {!ProfilePickerMainViewElement} */ (
        testElement.shadowRoot.querySelector('profile-picker-main-view'));
    await whenCheck(mainView, () => mainView.classList.contains('active'));
  });

  test('ForceSignInEnabled', async function() {
    loadTimeData.overrideValues({
      isProfileCreationAllowed: true,
      isForceSigninEnabled: true,
    });
    await resetTestElement(Routes.NEW_PROFILE);
    assertEquals(
        testElement.shadowRoot.querySelectorAll('[slot=view]').length, 1);
    const mainView = /** @type {!ProfilePickerMainViewElement} */ (
        testElement.shadowRoot.querySelector('profile-picker-main-view'));
    await whenCheck(mainView, () => mainView.classList.contains('active'));
    await browserProxy.whenCalled('loadSignInProfileCreationFlow');
  });
});
