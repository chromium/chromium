// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ensureLazyLoaded, ManageProfilesBrowserProxyImpl, navigateTo, Routes} from 'chrome://profile-picker/profile_picker.js';
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
        getComputedStyle(element.$$('#headerContainer'))
            .getPropertyValue('--theme-frame-color')
            .trim(),
        browserProxy.profileThemeInfo.themeFrameColor);
    assertEquals(
        getComputedStyle(element.$$('#headerContainer'))
            .getPropertyValue('--theme-text-color')
            .trim(),
        browserProxy.profileThemeInfo.themeFrameTextColor);
    assertEquals(
        getComputedStyle(element.$$('#headerContainer')).backgroundColor,
        browserProxy.profileThemeInfo.themeFrameColor);
    assertEquals(
        getComputedStyle(element.$$('#backButton'))
            .getPropertyValue('--cr-icon-button-fill-color')
            .trim(),
        browserProxy.profileThemeInfo.themeFrameTextColor);
  }

  test('ProfilePickerMainView', async function() {
    assertEquals(
        testElement.shadowRoot.querySelectorAll('[slot=view]').length, 1);
    const mainView = /** @type {!ProfilePickerMainViewElement} */ (
        testElement.$$('profile-picker-main-view'));
    await whenCheck(mainView, () => mainView.classList.contains('active'));
    await browserProxy.whenCalled('initializeMainView');
    assertTrue(mainView.$$('#wrapper').hidden);

    webUIListenerCallback(
        'profiles-list-changed', [browserProxy.profileSample]);
    flushTasks();
    assertEquals(
        mainView.$$('#wrapper').querySelectorAll('profile-card').length, 1);
    mainView.$$('#addProfile').click();
    await waitForProfileCretionLoad();
    assertEquals(
        testElement.shadowRoot.querySelectorAll('[slot=view]').length, 2);
    const choice = /** @type {!ProfileTypeChoiceElement} */ (
        testElement.$$('profile-type-choice'));
    assertTrue(!!choice);
    await whenCheck(choice, () => choice.classList.contains('active'));
    verifyProfileCreationViewStyle(choice);
  });

  test('SignInPromoSignIn', async function() {
    await resetTestElement(Routes.NEW_PROFILE);
    await waitForProfileCretionLoad();
    const choice = /** @type {!ProfileTypeChoiceElement} */ (
        testElement.$$('profile-type-choice'));
    assertTrue(!!choice);
    choice.$$('#signInButton').click();
    assertTrue(choice.$$('#signInButton').disabled);
    assertTrue(choice.$$('#notNowButton').disabled);
    assertTrue(choice.$$('#backButton').disabled);
    return browserProxy.whenCalled('loadSignInProfileCreationFlow');
  });

  test('ThemeColorConsistentInProfileCreationViews', async function() {
    await resetTestElement(Routes.NEW_PROFILE);
    await waitForProfileCretionLoad();
    const choice = /** @type {!ProfileTypeChoiceElement} */ (
        testElement.$$('profile-type-choice'));
    assertTrue(!!choice);
    await whenCheck(choice, () => choice.classList.contains('active'));
    verifyProfileCreationViewStyle(choice);
    choice.$$('#notNowButton').click();
    await waitBeforeNextRender(testElement);
    const customization =
        /** @type {!LocalProfileCustomizationElement} */ (
            testElement.$$('local-profile-customization'));
    assertTrue(!!customization);
    await whenCheck(
        customization, () => customization.classList.contains('active'));
    verifyProfileCreationViewStyle(customization);

    // Test color changes from the local profile customization is reflected in
    // the profile type choice.
    browserProxy.resetResolver('getProfileThemeInfo');
    const colorPicker = customization.$$('#colorPicker');
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
    customization.$$('#backButton').click();
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
        testElement.$$('profile-picker-main-view'));
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
        testElement.$$('profile-picker-main-view'));
    await whenCheck(mainView, () => mainView.classList.contains('active'));
    await browserProxy.whenCalled('loadSignInProfileCreationFlow');
  });
});
