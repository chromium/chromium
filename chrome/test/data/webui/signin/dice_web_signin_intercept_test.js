// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://signin-dice-web-intercept/dice_web_signin_intercept_app.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {AccountInfo, DiceWebSigninInterceptBrowserProxyImpl, InterceptionParameters} from 'chrome://signin-dice-web-intercept/dice_web_signin_intercept_browser_proxy.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {isChildVisible} from '../test_util.m.js';

import {TestDiceWebSigninInterceptBrowserProxy} from './test_dice_web_signin_intercept_browser_proxy.js';

/** @param {!InterceptionParameters} parameters */
function fireParametersChanged(parameters) {
  webUIListenerCallback('interception-parameters-changed', parameters);
}

suite('DiceWebSigninInterceptTest', function() {
  /** @type {!DiceWebSigninInterceptAppElement} */
  let app;

  /** @type {!TestDiceWebSigninInterceptBrowserProxy} */
  let browserProxy;

  /** @type {string} */
  const AVATAR_URL_1 = 'chrome://theme/IDR_PROFILE_AVATAR_1';
  /** @type {string} */
  const AVATAR_URL_2 = 'chrome://theme/IDR_PROFILE_AVATAR_2';

  setup(function() {
    browserProxy = new TestDiceWebSigninInterceptBrowserProxy();
    browserProxy.setInterceptionParameters({
      headerText: 'header_text',
      bodyTitle: 'body_title',
      bodyText: 'body_text',
      headerTextColor: 'rgba(255, 255, 255, 1)',
      headerBackgroundColor: 'rgba(255, 0, 0, 1)',
      interceptedAccount: {isManaged: false, pictureUrl: AVATAR_URL_1},
      primaryAccount: {isManaged: false, pictureUrl: AVATAR_URL_2}
    });
    DiceWebSigninInterceptBrowserProxyImpl.instance_ = browserProxy;
    document.body.innerHTML = '';
    app = /** @type {!DiceWebSigninInterceptAppElement} */ (
        document.createElement('dice-web-signin-intercept-app'));
    document.body.append(app);
    return browserProxy.whenCalled('pageLoaded');
  });

  /**
   * Checks that the text from the interception parameters is displayed.
   * @param {string} expectedHeaderText
   * @param {string} expectedBodyTitle
   * @param {string} expectedBodyText
   */
  function checkTextValues(
      expectedHeaderText, expectedBodyTitle, expectedBodyText) {
    const headerTextElement = app.$$('#headerText');
    assertEquals(expectedHeaderText, headerTextElement.textContent);
    const titleElement = app.$$('#title');
    assertEquals(expectedBodyTitle, titleElement.textContent);
    const contentsElement = app.$$('#contents');
    assertEquals(expectedBodyText, contentsElement.textContent);
  }

  function checkImageUrl(elementId, expectedUrl) {
    assertTrue(isChildVisible(app, elementId));
    const img = app.$$(elementId);
    assertEquals(expectedUrl, img.src);
  }

  test('ClickAccept', function() {
    assertTrue(isChildVisible(app, '#acceptButton'));
    app.$$('#acceptButton').click();
    return browserProxy.whenCalled('accept');
  });

  test('ClickCancel', function() {
    assertTrue(isChildVisible(app, '#cancelButton'));
    app.$$('#cancelButton').click();
    return browserProxy.whenCalled('cancel');
  });

  test('TextValues', function() {
    // Initial values.
    checkTextValues('header_text', 'body_title', 'body_text');

    // Update the values.
    fireParametersChanged({
      headerText: 'new_header_text',
      bodyTitle: 'new_body_title',
      bodyText: 'new_body_text',
      headerTextColor: 'rgba(255, 255, 255, 1)',
      headerBackgroundColor: 'rgba(255, 0, 0, 1)',
      interceptedAccount: {isManaged: false, pictureUrl: AVATAR_URL_1},
    });
    checkTextValues('new_header_text', 'new_body_title', 'new_body_text');
  });

  test('Avatars', function() {
    // Consumer avatars.
    checkImageUrl('#avatar', AVATAR_URL_1);
    assertFalse(isChildVisible(app, '#badge'));

    const parameters = {
      headerText: 'header_text',
      bodyTitle: 'body_title',
      bodyText: 'body_text',
      headerTextColor: 'rgba(255, 255, 255, 1)',
      headerBackgroundColor: 'rgba(255, 0, 0, 1)',
      interceptedAccount: {isManaged: false, pictureUrl: AVATAR_URL_2},
    };

    // Update urls.
    fireParametersChanged(parameters);
    checkImageUrl('#avatar', AVATAR_URL_2);

    // Update isManaged for intercepted account.
    parameters.interceptedAccount.isManaged = true;
    fireParametersChanged(parameters);
    assertTrue(isChildVisible(app, '#badge'));
    parameters.interceptedAccount.isManaged = false;
    fireParametersChanged(parameters);
    assertFalse(isChildVisible(app, '#badge'));
  });

});
