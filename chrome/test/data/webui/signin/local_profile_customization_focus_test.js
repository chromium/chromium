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

suite('LocalProfileCustomizationFocusTest', function() {
  /** @type {!ProfilePickerAppElement} */
  let testElement;

  /** @type {!TestManageProfilesBrowserProxy} */
  let browserProxy;

  async function resetTestElement(route) {
    document.body.innerHTML = '';
    navigateTo(route);
    testElement = /** @type {!ProfilePickerAppElement} */ (
        document.createElement('profile-picker-app'));
    document.body.appendChild(testElement);
    await waitBeforeNextRender(testElement);
  }

  setup(function() {
    browserProxy = new TestManageProfilesBrowserProxy();
    ManageProfilesBrowserProxyImpl.instance_ = browserProxy;
    return resetTestElement(Routes.MAIN);
  });


  /** @param {!ProfilePickerMainViewElement} mainView */
  async function setupMainView(mainView) {
    assertTrue(!!mainView);
    await whenCheck(mainView, () => mainView.classList.contains('active'));
    await browserProxy.whenCalled('initializeMainView');
    webUIListenerCallback(
        'profiles-list-changed', [browserProxy.profileSample]);
    flushTasks();
  }

  /** @param {!ProfilePickerMainViewElement} mainView */
  function navigateToProfileCreationFromMainView(mainView) {
    mainView.$$('#addProfile').focus();
    mainView.$$('#addProfile').click();
    flush();
  }

  async function setupProfileCreation() {
    await Promise.all([
      browserProxy.whenCalled('getNewProfileSuggestedThemeInfo'),
      ensureLazyLoaded(),
    ]);
    browserProxy.reset();
    flush();
    await waitBeforeNextRender(testElement);
  }

  /**
   * @param {boolean} focused
   * @param {boolean} valid
   */
  async function verifyProfileName(focused, valid) {
    const profileNameInput = /** @type {!CrInputElement} */ (
        testElement.$$('local-profile-customization').$$('#nameInput'));
    assertTrue(!!profileNameInput);
    await whenAttributeIs(profileNameInput, 'focused_', focused ? '' : null);
    assertEquals(!valid, profileNameInput.invalid);
  }

  test('ProfileCreationFlowWithSigninPromo', async function() {
    assertTrue(loadTimeData.getValue('isBrowserSigninAllowed'));
    navigateTo(Routes.NEW_PROFILE);
    await setupProfileCreation();
    const choice = /** @type {!ProfileTypeChoiceElement} */ (
        testElement.$$('profile-type-choice'));
    assertTrue(!!choice);
    await whenCheck(choice, () => choice.classList.contains('active'));
    choice.$$('#notNowButton').focus();
    choice.$$('#notNowButton').click();
    flush();
    await waitBeforeNextRender(testElement);
    const customization =
        /** @type {!LocalProfileCustomizationElement} */ (
            testElement.$$('local-profile-customization'));
    assertTrue(!!customization);
    await whenCheck(
        customization, () => customization.classList.contains('active'));
    await verifyProfileName(true, true);

    // Invalid profile name.
    customization.$$('#nameInput').value = '  ';
    await verifyProfileName(true, false);
    customization.$$('#backButton').focus();
    await verifyProfileName(false, false);

    // Navigate back and in again.
    customization.$$('#backButton').click();
    flush();
    await whenCheck(choice, () => choice.classList.contains('active'));
    choice.$$('#notNowButton').focus();
    choice.$$('#notNowButton').click();
    flush();
    await whenCheck(
        customization, () => customization.classList.contains('active'));
    await verifyProfileName(true, false);
    customization.$$('#nameInput').value = 'Work';
    assertFalse(customization.$$('#nameInput').invalid);
  });

  test('BrowserSigninNotAllowed', async function() {
    loadTimeData.overrideValues({
      isBrowserSigninAllowed: false,
    });
    await resetTestElement(Routes.MAIN);
    const mainView = /** @type {!ProfilePickerMainViewElement} */ (
        testElement.$$('profile-picker-main-view'));
    await setupMainView(mainView);
    navigateToProfileCreationFromMainView(mainView);
    await setupProfileCreation();
    let customization =
        /** @type {!LocalProfileCustomizationElement} */ (
            testElement.$$('local-profile-customization'));
    await whenCheck(
        customization, () => customization.classList.contains('active'));
    await verifyProfileName(true, true);
    customization.$$('#backButton').focus();
    await verifyProfileName(false, false);
    customization.$$('#backButton').click();
    flush();
    await whenCheck(mainView, () => mainView.classList.contains('active'));
    navigateToProfileCreationFromMainView(mainView);
    await whenCheck(
        customization, () => customization.classList.contains('active'));

    // The invalid state should not stick if the profile name is empty.
    await verifyProfileName(true, true);

    // Open the profile creation flow directly.
    await resetTestElement(Routes.NEW_PROFILE);
    await setupProfileCreation();
    customization =
        /** @type {!LocalProfileCustomizationElement} */ (
            testElement.$$('local-profile-customization'));
    await whenCheck(
        customization, () => customization.classList.contains('active'));
    await verifyProfileName(true, true);
  });
});
