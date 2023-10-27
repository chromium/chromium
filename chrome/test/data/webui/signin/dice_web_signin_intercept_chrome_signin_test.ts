// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://signin-dice-web-intercept/chrome_signin/chrome_signin_app.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {ChromeSigninAppElement} from 'chrome://signin-dice-web-intercept/chrome_signin/chrome_signin_app.js';
import {ChromeSigninInterceptionParameters, DiceWebSigninInterceptBrowserProxyImpl} from 'chrome://signin-dice-web-intercept/dice_web_signin_intercept_browser_proxy.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';

import {TestDiceWebSigninInterceptBrowserProxy} from './test_dice_web_signin_intercept_browser_proxy.js';

const AVATAR_URL: string = 'chrome://theme/IDR_PROFILE_AVATAR_1';

const PARAMETERS: ChromeSigninInterceptionParameters = {
  fullName: 'full_name',
  givenName: 'given_name',
  email: 'email@example.com',
  pictureUrl: AVATAR_URL,
};

suite('DiceWebSigninInterceptChromeSigninTest', function() {
  let app: ChromeSigninAppElement;
  let browserProxy: TestDiceWebSigninInterceptBrowserProxy;

  setup(async function() {
    browserProxy = new TestDiceWebSigninInterceptBrowserProxy();
    browserProxy.setChromeSigninInterceptionParameters(PARAMETERS);
    DiceWebSigninInterceptBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('chrome-signin-app');
    document.body.append(app);
    await waitAfterNextRender(app);
    return browserProxy.whenCalled('chromeSigninPageLoaded');
  });

  test('ClickAccept', function() {
    assertTrue(isChildVisible(app, '#accept-button'));
    app.shadowRoot!.querySelector<CrButtonElement>('#accept-button')!.click();
    return browserProxy.whenCalled('accept');
  });

  test('ClickCancel', function() {
    assertTrue(isChildVisible(app, '#cancel-button'));
    app.shadowRoot!.querySelector<CrButtonElement>('#cancel-button')!.click();
    return browserProxy.whenCalled('cancel');
  });

  test('AppContentValues', function() {
    const titleElement = app.shadowRoot!.querySelector('#title')!;
    assertEquals(app.i18n('chromeSigninTitle'), titleElement.textContent);
    const subtitleElement = app.shadowRoot!.querySelector('#subtitle')!;
    assertEquals(app.i18n('chromeSigninSubtitle'), subtitleElement.textContent);
    const nameElement = app.shadowRoot!.querySelector('#name')!;
    assertEquals(PARAMETERS.fullName, nameElement.textContent);
    const emailElement = app.shadowRoot!.querySelector('#email')!;
    assertEquals(PARAMETERS.email, emailElement.textContent);
    const acceptButton = app.shadowRoot!.querySelector('#accept-button')!;
    assertEquals(
        app.i18n('chromeSigninAcceptText', PARAMETERS.givenName),
        acceptButton.textContent!.trim());
    const cancelButton = app.shadowRoot!.querySelector('#cancel-button')!;
    assertEquals(
        app.i18n('chromeSigninDeclineText'), cancelButton.textContent!.trim());

    assertTrue(isChildVisible(app, '#account-icon'));
    const img =
        app.shadowRoot!.querySelector<HTMLImageElement>('#account-icon')!;
    assertEquals(PARAMETERS.pictureUrl, img.src);

    // Simulate a change of picture url.
    const new_params = {
      ...PARAMETERS,
      pictureUrl: 'chrome://theme/IDR_PROFILE_AVATAR_2',
    };
    webUIListenerCallback(
        'interception-chrome-signin-parameters-changed', new_params);

    // It should be reflected in the UI.
    assertEquals(new_params.pictureUrl, img.src);
  });
});
