// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ensureLazyLoaded, ManageProfilesBrowserProxyImpl, navigateTo, Routes} from 'chrome://profile-picker/profile_picker.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertTrue} from '../chai_assert.js';
import {waitBeforeNextRender} from '../test_util.m.js';

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
  function waitForProfileCretionLoad() {
    return Promise.all([
      browserProxy.whenCalled('getNewProfileSuggestedThemeInfo'),
      ensureLazyLoaded(),
    ]);
  }

  test('ProfilePickerMainView', async function() {
    assertEquals(
        testElement.shadowRoot.querySelectorAll('[slot=view]').length, 1);
    const mainView = /** @type {!ProfilePickerMainViewElement} */ (
        testElement.$$('profile-picker-main-view'));
    assertTrue(mainView.classList.contains('active'));
    await browserProxy.whenCalled('initializeMainView');
    assertTrue(mainView.$$('#wrapper').hidden);
    const profile = {
      profilePath: 'profile1',
      localProfileName: 'Work',
      isSyncing: true,
      gaiaName: 'Alice',
      userName: 'Alice@gmail.com',
      isManaged: false,
      avatarIcon: 'url',
    };
    webUIListenerCallback('profiles-list-changed', [profile]);
    flush();
    assertEquals(
        mainView.shadowRoot.querySelectorAll('profile-card').length, 1);
    mainView.$$('#addProfile').querySelectorAll('cr-icon-button')[0].click();
    await waitForProfileCretionLoad();
    await waitBeforeNextRender(testElement);
    assertEquals(
        testElement.shadowRoot.querySelectorAll('[slot=view]').length, 2);
    assertTrue(!mainView.classList.contains('active'));
  });

  test('SignInPromoSignIn', async function() {
    resetTestElement(Routes.NEW_PROFILE);
    await waitForProfileCretionLoad();
    await waitBeforeNextRender(testElement);
    const choice = /** @type {!ProfileTypeChoiceElement} */ (
        testElement.$$('profile-type-choice'));
    assertTrue(!!choice);
    choice.$$('#signInButton').click();
    assertTrue(choice.$$('#signInButton').disabled);
    assertTrue(choice.$$('#notNowButton').disabled);
    assertTrue(choice.$$('#backButton').disabled);
    return browserProxy.whenCalled('loadSignInProfileCreationFlow');
  });

  test('SignInPromoLocalProfile', async function() {
    resetTestElement(Routes.NEW_PROFILE);
    await waitForProfileCretionLoad();
    await waitBeforeNextRender(testElement);

    const choice = /** @type {!ProfileTypeChoiceElement} */ (
        testElement.$$('profile-type-choice'));
    assertTrue(!!choice);
    choice.$$('#notNowButton').click();
    const customization =
        /** @type {!LocalProfileCustomizationElement} */ (
            testElement.$$('local-profile-customization'));
    assertTrue(!!customization);
    assertTrue(customization.classList.contains('active'));
  });

  test('ProfileCreationNotAllowed', async function() {
    document.body.innerHTML = '';
    loadTimeData.overrideValues({
      isProfileCreationAllowed: false,
    });
    resetTestElement(Routes.NEW_PROFILE);
    assertEquals(
        testElement.shadowRoot.querySelectorAll('[slot=view]').length, 1);
    const mainView = /** @type {!ProfilePickerMainViewElement} */ (
        testElement.$$('profile-picker-main-view'));
    await waitBeforeNextRender(testElement);
    assertTrue(mainView.classList.contains('active'));
  });
});
