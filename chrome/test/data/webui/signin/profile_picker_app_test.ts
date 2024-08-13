// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://profile-picker/profile_picker.js';
// <if expr="chromeos_lacros">
import 'chrome://profile-picker/lazy_load.js';

// </if>

import type {
  // <if expr="chromeos_lacros">
  AvailableAccount,
  // </if>
  ProfilePickerAppElement} from 'chrome://profile-picker/profile_picker.js';
import {ensureLazyLoaded, ManageProfilesBrowserProxyImpl, navigateTo, Routes} from 'chrome://profile-picker/profile_picker.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished, whenCheck} from 'chrome://webui-test/test_util.js';

import {TestManageProfilesBrowserProxy} from './test_manage_profiles_browser_proxy.js';

suite('ProfilePickerAppTest', function() {
  let testElement: ProfilePickerAppElement;
  let browserProxy: TestManageProfilesBrowserProxy;

  function resetTestElement(route: Routes) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    navigateTo(route);
    testElement = document.createElement('profile-picker-app');
    document.body.appendChild(testElement);
  }

  setup(function() {
    browserProxy = new TestManageProfilesBrowserProxy();
    ManageProfilesBrowserProxyImpl.setInstance(browserProxy);

    return resetTestElement(Routes.MAIN);
  });

  /**
   * @return Promise that resolves when initialization is complete
   *     and the lazy loaded module has been loaded.
   */
  async function waitForProfileCreationLoad(): Promise<void> {
    await Promise.all([
      browserProxy.whenCalled('getNewProfileSuggestedThemeInfo'),
      ensureLazyLoaded(),
    ]);
    browserProxy.reset();
  }

  function verifyProfileCreationViewStyle(element: HTMLElement) {
    assertEquals(
        getComputedStyle(element.shadowRoot!.querySelector('#headerContainer')!)
            .getPropertyValue('--theme-frame-color')
            .trim(),
        browserProxy.profileThemeInfo.themeFrameColor);
    assertEquals(
        getComputedStyle(element.shadowRoot!.querySelector('#headerContainer')!)
            .getPropertyValue('--theme-text-color')
            .trim(),
        browserProxy.profileThemeInfo.themeFrameTextColor);
  }

  test('ProfilePickerMainView', async function() {
    assertEquals(
        testElement.shadowRoot!.querySelectorAll('[slot=view]').length, 1);
    const mainView =
        testElement.shadowRoot!.querySelector('profile-picker-main-view')!;
    await whenCheck(mainView, () => mainView.classList.contains('active'));
    await browserProxy.whenCalled('initializeMainView');
    const profilesContainer =
        mainView.shadowRoot!.querySelector<HTMLElement>('#wrapper')!
            .querySelector<HTMLElement>('#profilesContainer')!;
    assertTrue(profilesContainer.hidden);

    webUIListenerCallback(
        'profiles-list-changed', [browserProxy.profileSample]);
    await microtasksFinished();
    assertEquals(profilesContainer.querySelectorAll('profile-card').length, 1);
    mainView.$.addProfile.click();
    await waitForProfileCreationLoad();
    assertEquals(
        testElement.shadowRoot!.querySelectorAll('[slot=view]').length, 2);
    const choice = testElement.shadowRoot!.querySelector('profile-type-choice');
    assertTrue(!!choice);
    await whenCheck(choice!, () => choice.classList.contains('active'));
    verifyProfileCreationViewStyle(choice!);
  });

  // <if expr="chromeos_lacros">
  test('SignInPromoSignInWithAvailableAccountLacros', async function() {
    await resetTestElement(Routes.NEW_PROFILE);
    await waitForProfileCreationLoad();
    const choice =
        testElement.shadowRoot!.querySelector('profile-type-choice')!;
    assertTrue(!!choice);
    // Add available account to trigger the account selection screen.
    const availableAccount: AvailableAccount = {
      gaiaId: 'available-id',
      name: 'Account Name',
      email: 'email@gmail.com',
      accountImageUrl: 'account-image-url',
    };
    webUIListenerCallback('available-accounts-changed', [availableAccount]);
    await microtasksFinished();
    choice.$.signInButton.click();
    // Start Lacros signin flow.
    await microtasksFinished();
    const accountSelectionLacros =
        testElement.shadowRoot!.querySelector('account-selection-lacros');
    assertTrue(!!accountSelectionLacros);
    // Test the back button.
    const backButton =
        accountSelectionLacros.shadowRoot!.querySelector<HTMLElement>(
            '#backButton');
    assertTrue(!!backButton);
    backButton.click();
    await whenCheck(choice, () => choice.classList.contains('active'));
  });

  test('SignInPromoSignInWithoutAccountLacros', async function() {
    await resetTestElement(Routes.NEW_PROFILE);
    await waitForProfileCreationLoad();
    const choice = testElement.shadowRoot!.querySelector('profile-type-choice');
    assertTrue(!!choice);
    // No available account.
    webUIListenerCallback('available-accounts-changed', []);
    await microtasksFinished();
    choice.$.signInButton.click();
    return browserProxy.whenCalled('selectNewAccount');
  });
  // </if>

  test('SignInPromoSignIn', async function() {
    await resetTestElement(Routes.NEW_PROFILE);
    await waitForProfileCreationLoad();
    const choice = testElement.shadowRoot!.querySelector('profile-type-choice');
    assertTrue(!!choice);
    choice.$.signInButton.click();
    await microtasksFinished();
    assertTrue(choice.$.signInButton.disabled);
    assertTrue(choice.$.notNowButton.disabled);
    assertTrue(choice.$.backButton.disabled);
    return browserProxy.whenCalled('selectNewAccount');
  });

  test('ProfileCreationNotAllowed', async function() {
    loadTimeData.overrideValues({
      isProfileCreationAllowed: false,
    });
    await resetTestElement(Routes.NEW_PROFILE);
    assertEquals(
        testElement.shadowRoot!.querySelectorAll('[slot=view]').length, 1);
    const mainView =
        testElement.shadowRoot!.querySelector('profile-picker-main-view')!;
    await whenCheck(mainView, () => mainView.classList.contains('active'));
  });

  test('ForceSignInEnabled', async function() {
    loadTimeData.overrideValues({
      isProfileCreationAllowed: true,
      isForceSigninEnabled: true,
    });
    await resetTestElement(Routes.NEW_PROFILE);
    assertEquals(
        testElement.shadowRoot!.querySelectorAll('[slot=view]').length, 1);
    const mainView =
        testElement.shadowRoot!.querySelector('profile-picker-main-view')!;
    await whenCheck(mainView, () => mainView.classList.contains('active'));
    await browserProxy.whenCalled('selectNewAccount');
  });

  test('CreateLocalProfile', async function() {
    loadTimeData.overrideValues({
      isProfileCreationAllowed: true,
      isLocalProfileCreationDialogEnabled: true,
      isForceSigninEnabled: false,
    });
    await resetTestElement(Routes.NEW_PROFILE);
    await waitForProfileCreationLoad();
    const choice = testElement.shadowRoot!.querySelector('profile-type-choice');
    assertTrue(!!choice);
    await whenCheck(choice!, () => choice.classList.contains('active'));
    verifyProfileCreationViewStyle(choice);
    choice.$.notNowButton.click();
    await microtasksFinished();
    const args = await browserProxy.whenCalled('continueWithoutAccount');
    assertEquals(args[0], browserProxy.profileThemeInfo.color);
    assertTrue(testElement.profileCreationInProgress);
    assertTrue(choice.profileCreationInProgress);
    assertTrue(choice.$.signInButton.disabled);
    assertTrue(choice.$.notNowButton.disabled);
    assertTrue(choice.$.backButton.disabled);

    webUIListenerCallback('create-profile-finished', []);
    await microtasksFinished();
    assertFalse(testElement.profileCreationInProgress);
    assertFalse(choice.profileCreationInProgress);
    assertFalse(choice.$.signInButton.disabled);
    assertFalse(choice.$.notNowButton.disabled);
    assertFalse(choice.$.backButton.disabled);
  });

  test('CreateLocalProfileWithBrowserSigninNotAllowed', async function() {
    loadTimeData.overrideValues({
      isProfileCreationAllowed: true,
      isLocalProfileCreationDialogEnabled: true,
      isForceSigninEnabled: false,
      isBrowserSigninAllowed: false,
    });
    await resetTestElement(Routes.NEW_PROFILE);
    await browserProxy.whenCalled('getNewProfileSuggestedThemeInfo');
    const args = await browserProxy.whenCalled('continueWithoutAccount');
    assertEquals(args[0], browserProxy.profileThemeInfo.color);
  });
});
