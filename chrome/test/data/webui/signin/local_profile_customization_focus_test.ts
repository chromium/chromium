// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://profile-picker/profile_picker.js';

import {ensureLazyLoaded, ManageProfilesBrowserProxyImpl, navigateTo, ProfilePickerAppElement, ProfilePickerMainViewElement, Routes} from 'chrome://profile-picker/profile_picker.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {whenAttributeIs, whenCheck} from 'chrome://webui-test/test_util.js';

import {TestManageProfilesBrowserProxy} from './test_manage_profiles_browser_proxy.js';

suite('LocalProfileCustomizationFocusTest', function() {
  let testElement: ProfilePickerAppElement;
  let browserProxy: TestManageProfilesBrowserProxy;

  async function resetTestElement(route: Routes) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    navigateTo(route);
    testElement = document.createElement('profile-picker-app');
    document.body.appendChild(testElement);
    await waitBeforeNextRender(testElement);
  }

  setup(function() {
    loadTimeData.overrideValues({
      isLocalProfileCreationDialogEnabled: false,
    });
    browserProxy = new TestManageProfilesBrowserProxy();
    ManageProfilesBrowserProxyImpl.setInstance(browserProxy);
    return resetTestElement(Routes.MAIN);
  });


  async function setupMainView(mainView: ProfilePickerMainViewElement) {
    assertTrue(!!mainView);
    await whenCheck(mainView, () => mainView.classList.contains('active'));
    await browserProxy.whenCalled('initializeMainView');
    webUIListenerCallback(
        'profiles-list-changed', [browserProxy.profileSample]);
    flushTasks();
  }

  function navigateToProfileCreationFromMainView(
      mainView: ProfilePickerMainViewElement) {
    mainView.$.addProfile.focus();
    mainView.$.addProfile.click();
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

  async function verifyProfileName(focused: boolean, valid: boolean) {
    const profileNameInput =
        testElement.shadowRoot!.querySelector(
                                   'local-profile-customization')!.$.nameInput;
    assertTrue(!!profileNameInput);
    await whenAttributeIs(profileNameInput, 'focused_', focused ? '' : null);
    assertEquals(!valid, profileNameInput.invalid);
  }

  test('ProfileCreationFlowWithSigninPromo', async function() {
    assertTrue(loadTimeData.getValue('isBrowserSigninAllowed'));
    await resetTestElement(Routes.NEW_PROFILE);
    await setupProfileCreation();
    const choice = testElement.shadowRoot!.querySelector('profile-type-choice');
    assertTrue(!!choice);
    await whenCheck(choice!, () => choice!.classList.contains('active'));

    const notNowButton = choice!.$.notNowButton;
    notNowButton.focus();
    notNowButton.click();
    flush();
    await waitBeforeNextRender(testElement);
    const customization =
        testElement.shadowRoot!.querySelector('local-profile-customization');
    assertTrue(!!customization);
    await whenCheck(
        customization!, () => customization!.classList.contains('active'));
    await verifyProfileName(true, true);

    // Invalid profile name.
    customization!.$.nameInput.value = '  ';
    await verifyProfileName(true, false);
    customization!.$.backButton.focus();
    await verifyProfileName(false, false);

    // Navigate back and in again.
    choice!.$.backButton.click();
    flush();
    await whenCheck(choice!, () => choice!.classList.contains('active'));
    choice!.$.notNowButton.focus();
    choice!.$.notNowButton.click();
    flush();
    await whenCheck(
        customization!, () => customization!.classList.contains('active'));
    await verifyProfileName(true, false);
    customization!.$.nameInput.value = 'Work';
    assertFalse(customization!.$.nameInput.invalid);
  });

  test('BrowserSigninNotAllowed', async function() {
    loadTimeData.overrideValues({
      isBrowserSigninAllowed: false,
    });
    await resetTestElement(Routes.MAIN);
    const mainView =
        testElement.shadowRoot!.querySelector('profile-picker-main-view')!;
    await setupMainView(mainView);
    navigateToProfileCreationFromMainView(mainView);
    await setupProfileCreation();
    let customization =
        testElement.shadowRoot!.querySelector('local-profile-customization')!;
    await whenCheck(
        customization, () => customization.classList.contains('active'));
    await verifyProfileName(true, true);
    customization.$.backButton.focus();
    await verifyProfileName(false, false);
    customization.$.backButton.click();
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
        testElement.shadowRoot!.querySelector('local-profile-customization')!;
    await whenCheck(
        customization, () => customization.classList.contains('active'));
    await verifyProfileName(true, true);
  });
});
