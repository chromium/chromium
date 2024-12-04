// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://signin-dice-web-intercept.top-chrome/dice_web_signin_intercept_app.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {DiceWebSigninInterceptAppElement} from 'chrome://signin-dice-web-intercept.top-chrome/dice_web_signin_intercept_app.js';
import type {InterceptionParameters} from 'chrome://signin-dice-web-intercept.top-chrome/dice_web_signin_intercept_browser_proxy.js';
import {DiceWebSigninInterceptBrowserProxyImpl} from 'chrome://signin-dice-web-intercept.top-chrome/dice_web_signin_intercept_browser_proxy.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestDiceWebSigninInterceptBrowserProxy} from './test_dice_web_signin_intercept_browser_proxy.js';

const AVATAR_URL_1: string = 'chrome://theme/IDR_PROFILE_AVATAR_1';
const AVATAR_URL_2: string = 'chrome://theme/IDR_PROFILE_AVATAR_2';
const AVATAR_BADGE_SOURCE: string = 'cr:domain';

const BASE_PARAMETERS: InterceptionParameters = {
  headerText: 'header_text',
  bodyTitle: 'body_title',
  bodyText: 'body_text',
  confirmButtonLabel: 'confirm_label',
  cancelButtonLabel: 'cancel_label',
  managedDisclaimerText: 'managed_disclaimer',
  headerTextColor: 'rgba(255, 255, 255, 1)',
  interceptedProfileColor: 'rgba(255, 0, 0, 1)',
  primaryProfileColor: 'rgba(255, 255, 255, 1)',
  interceptedAccount: {pictureUrl: AVATAR_URL_1, avatarBadge: ''},
  primaryAccount: {pictureUrl: AVATAR_URL_2, avatarBadge: ''},
  useV2Design: false,
  showManagedDisclaimer: false,
};

function fireParametersChanged(parameters: InterceptionParameters) {
  webUIListenerCallback('interception-parameters-changed', parameters);
}

suite('DiceWebSigninInterceptTest', function() {
  let app: DiceWebSigninInterceptAppElement;
  let browserProxy: TestDiceWebSigninInterceptBrowserProxy;

  const PARAMETERS = BASE_PARAMETERS;

  setup(async function() {
    browserProxy = new TestDiceWebSigninInterceptBrowserProxy();
    browserProxy.setInterceptionParameters(PARAMETERS);
    DiceWebSigninInterceptBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('dice-web-signin-intercept-app');
    document.body.append(app);
    return browserProxy.whenCalled('pageLoaded');
  });

  /**
   * Checks that the text from the interception parameters is displayed.
   */
  function checkTextValues(
      expectedHeaderText: string, expectedBodyTitle: string,
      expectedBodyText: string, expectedConfirmLabel: string,
      expectedCancelLabel: string) {
    const headerTextElement = app.shadowRoot!.querySelector('#headerText')!;
    assertEquals(expectedHeaderText, headerTextElement.textContent);
    const titleElement = app.shadowRoot!.querySelector('#title')!;
    assertEquals(expectedBodyTitle, titleElement.textContent);
    const contentsElement = app.shadowRoot!.querySelector('#contents')!;
    assertEquals(expectedBodyText, contentsElement.textContent);
    const confirmButton = app.$.acceptButton;
    assertEquals(expectedConfirmLabel, confirmButton.textContent!.trim());
    const cancelButton = app.$.cancelButton;
    assertEquals(expectedCancelLabel, cancelButton.textContent!.trim());
  }

  function checkImageUrl(elementId: string, expectedUrl: string) {
    assertTrue(isChildVisible(app, elementId));
    const img = app.shadowRoot!.querySelector<HTMLImageElement>(elementId)!;
    assertEquals(expectedUrl, img.src);
  }

  test('ClickAccept', async function() {
    assertTrue(isChildVisible(app, '#acceptButton'));
    const acceptButton = app.$.acceptButton;
    const cancelButton = app.$.cancelButton;
    assertFalse(!!app.shadowRoot!.querySelector('.spinner'));
    assertFalse(acceptButton.disabled);
    assertFalse(cancelButton.disabled);

    acceptButton.click();
    await microtasksFinished();

    // Buttons are disabled and the spinner is active.
    assertTrue(acceptButton.disabled);
    assertTrue(cancelButton.disabled);
    assertTrue(!!app.shadowRoot!.querySelector('.spinner'));
    return browserProxy.whenCalled('accept');
  });

  test('ClickCancel', function() {
    assertTrue(isChildVisible(app, '#cancelButton'));
    app.$.cancelButton.click();
    return browserProxy.whenCalled('cancel');
  });

  test('TextValues', async function() {
    // Initial values.
    checkTextValues(
        'header_text', 'body_title', 'body_text', 'confirm_label',
        'cancel_label');

    // Update the values.
    fireParametersChanged({
      ...PARAMETERS,
      headerText: 'new_header_text',
      bodyTitle: 'new_body_title',
      bodyText: 'new_body_text',
      confirmButtonLabel: 'new_confirm_label',
      cancelButtonLabel: 'new_cancel_label',
    });
    await microtasksFinished();
    checkTextValues(
        'new_header_text', 'new_body_title', 'new_body_text',
        'new_confirm_label', 'new_cancel_label');
  });

  test('Avatars', async function() {
    const avatarSelector = '#avatarContainer>img';
    const badgeSelector = '#badge';


    // Consumer avatars.
    checkImageUrl(avatarSelector, AVATAR_URL_1);
    assertFalse(isChildVisible(app, '#badge'));

    let parameters = {
      ...PARAMETERS,
      interceptedAccount: {pictureUrl: AVATAR_URL_2, avatarBadge: ''},
      primaryAccount: {pictureUrl: AVATAR_URL_1, avatarBadge: ''},
      useV2Design: false,
    };

    // Update urls.
    fireParametersChanged(parameters);
    await microtasksFinished();

    assertTrue(isChildVisible(app, avatarSelector));
    checkImageUrl(avatarSelector, AVATAR_URL_2);

    // Update Management for intercepted account.
    parameters = structuredClone(parameters);
    parameters.interceptedAccount.avatarBadge = AVATAR_BADGE_SOURCE;
    fireParametersChanged(parameters);
    await microtasksFinished();
    assertTrue(isChildVisible(app, badgeSelector));
    parameters = structuredClone(parameters);
    parameters.interceptedAccount.avatarBadge = '';
    fireParametersChanged(parameters);
    await microtasksFinished();
    assertFalse(isChildVisible(app, badgeSelector));
  });

  test('ManagedDisclaimer', async function() {
    assertFalse(isChildVisible(app, '#managedDisclaimer'));

    // Update interceptedAccount but not showManagedDisclaimer and check that
    // the disclaimer is not shown. Equivalent to Sign-in Intercept Bubble V1
    // without Sync Promo.
    let parameters = {
      ...PARAMETERS,
      interceptedAccount:
          {avatarBadge: AVATAR_BADGE_SOURCE, pictureUrl: AVATAR_URL_1},
    };
    fireParametersChanged(parameters);
    await microtasksFinished();
    assertFalse(isChildVisible(app, '#managedDisclaimer'));

    // Update showManagedDisclaimer and check that the disclaimer is shown.
    // Equivalent to Sign-in Intercept Bubble V1 with Sync Promo.
    parameters = {
      ...PARAMETERS,
      interceptedAccount:
          {avatarBadge: AVATAR_BADGE_SOURCE, pictureUrl: AVATAR_URL_1},
      showManagedDisclaimer: true,
    };
    fireParametersChanged(parameters);
    await microtasksFinished();

    const managedDisclaimerElement =
        app.shadowRoot!.querySelector('#managedDisclaimer')!;
    assertTrue(isVisible(managedDisclaimerElement));
    assertEquals(
        'managed_disclaimer', managedDisclaimerElement.textContent!.trim());
  });
});

suite('DiceWebSigninInterceptTestV2', function() {
  let app: DiceWebSigninInterceptAppElement;
  let browserProxy: TestDiceWebSigninInterceptBrowserProxy;

  const PARAMETERS:
      InterceptionParameters = {...BASE_PARAMETERS, useV2Design: true};

  setup(async function() {
    browserProxy = new TestDiceWebSigninInterceptBrowserProxy();
    browserProxy.setInterceptionParameters(PARAMETERS);
    DiceWebSigninInterceptBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('dice-web-signin-intercept-app');
    document.body.append(app);
    return browserProxy.whenCalled('pageLoaded');
  });

  function checkImageUrl(elementId: string, expectedUrl: string) {
    assertTrue(isChildVisible(app, elementId));
    const img = app.shadowRoot!.querySelector<HTMLImageElement>(elementId)!;
    assertEquals(expectedUrl, img.src);
  }

  test('AvatarsV2', async function() {
    const interceptedAvatarSelector = '#avatarIntercepted>img';
    const primaryAvatarSelector = '#avatarPrimary>img';
    const interceptedBadgeSelector = '#avatarIntercepted>.work-badge';
    const primaryBadgeSelector = '#avatarPrimary>.work-badge';

    // V1 header should not be visible
    assertFalse(isChildVisible(app, '#header'));

    // Consumer avatars.
    checkImageUrl(interceptedAvatarSelector, AVATAR_URL_1);
    assertFalse(isChildVisible(app, interceptedBadgeSelector));

    checkImageUrl(primaryAvatarSelector, AVATAR_URL_2);
    assertFalse(isChildVisible(app, interceptedBadgeSelector));

    // Update urls.
    let parameters = {
      ...PARAMETERS,
      interceptedAccount: {pictureUrl: AVATAR_URL_2, avatarBadge: ''},
      primaryAccount: {pictureUrl: AVATAR_URL_1, avatarBadge: ''},
    };
    fireParametersChanged(parameters);
    await microtasksFinished();
    checkImageUrl(interceptedAvatarSelector, AVATAR_URL_2);
    checkImageUrl(primaryAvatarSelector, AVATAR_URL_1);

    // Update Managed account information back and forth..
    parameters = structuredClone(parameters);
    parameters.interceptedAccount.avatarBadge = AVATAR_BADGE_SOURCE;
    fireParametersChanged(parameters);
    await microtasksFinished();
    assertTrue(isChildVisible(app, interceptedBadgeSelector));
    assertFalse(isChildVisible(app, primaryBadgeSelector));

    parameters = structuredClone(parameters);
    parameters.primaryAccount.avatarBadge = AVATAR_BADGE_SOURCE;
    fireParametersChanged(parameters);
    await microtasksFinished();
    assertTrue(isChildVisible(app, interceptedBadgeSelector));
    assertTrue(isChildVisible(app, primaryBadgeSelector));

    parameters = structuredClone(parameters);
    parameters.interceptedAccount.avatarBadge = '';
    fireParametersChanged(parameters);
    await microtasksFinished();
    assertFalse(isChildVisible(app, interceptedBadgeSelector));
    assertTrue(isChildVisible(app, primaryBadgeSelector));

    parameters = structuredClone(parameters);
    parameters.primaryAccount.avatarBadge = '';
    fireParametersChanged(parameters);
    await microtasksFinished();
    assertFalse(isChildVisible(app, interceptedBadgeSelector));
    assertFalse(isChildVisible(app, primaryBadgeSelector));
  });

  test('ManagedDisclaimer', async function() {
    assertFalse(isChildVisible(app, '#managedDisclaimer'));

    // Update showManagedDisclaimer and check that the disclaimer is shown.
    const parameters = {
      ...PARAMETERS,
      interceptedAccount:
          {avatarBadge: AVATAR_BADGE_SOURCE, pictureUrl: AVATAR_URL_1},
      showManagedDisclaimer: true,
    };
    fireParametersChanged(parameters);
    await microtasksFinished();

    const managedDisclaimerElement =
        app.shadowRoot!.querySelector('#managedDisclaimer')!;
    assertTrue(isVisible(managedDisclaimerElement));
    assertEquals(
        'managed_disclaimer', managedDisclaimerElement.textContent!.trim());
  });
});
