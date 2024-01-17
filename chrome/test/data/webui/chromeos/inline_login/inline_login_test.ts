// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://chrome-signin/inline_login_app.js';

import {InlineLoginAppElement, View} from 'chrome://chrome-signin/inline_login_app.js';
import {InlineLoginBrowserProxyImpl} from 'chrome://chrome-signin/inline_login_browser_proxy.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isChromeOS} from 'chrome://resources/js/platform.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';

import {fakeAuthenticationData, getFakeAccountsList, getFakeDeviceId, TestAuthenticator, TestInlineLoginBrowserProxy} from './inline_login_test_util.js';

suite('InlineLoginTest', () => {
  let inlineLoginComponent: InlineLoginAppElement;
  let testBrowserProxy: TestInlineLoginBrowserProxy;
  let testAuthenticator: TestAuthenticator;

  function isVisible(selector: string): boolean {
    if (!inlineLoginComponent) {
      return false;
    }
    return isChildVisible(inlineLoginComponent, selector, false);
  }

  suiteSetup(function() {
    // This test suite tests the 'default' behavior of inline login page, when
    // only 'Add account' screen is shown: this happens on Chrome Desktop or
    // when `kInlineLoginWelcomePageSkipped` pref is set to true.
    loadTimeData.overrideValues({shouldSkipWelcomePage: true});
  });

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testBrowserProxy = new TestInlineLoginBrowserProxy();
    InlineLoginBrowserProxyImpl.setInstance(testBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    inlineLoginComponent = document.createElement('inline-login-app');
    document.body.appendChild(inlineLoginComponent);
    testAuthenticator = new TestAuthenticator();
    inlineLoginComponent.setAuthenticatorForTest(testAuthenticator);
    flush();
  });

  test('Initialize', () => {
    webUIListenerCallback('load-authenticator', fakeAuthenticationData);
    // 'Add account' screen should be shown.
    assertTrue(isVisible(`#${View.ADD_ACCOUNT}`));

    // 'Welcome' screen should be hidden.
    assertFalse(isVisible(`#${View.WELCOME}`));

    // The 'loading' spinner should be shown and 'initialize' should be called
    // on startup.
    assertTrue(inlineLoginComponent.$.spinner.active);
    assertTrue(inlineLoginComponent.$.signinFrame.hidden);
    assertEquals(1, testBrowserProxy.getCallCount('initialize'));
  });

  test('WebUICallbacks', () => {
    webUIListenerCallback('load-authenticator', fakeAuthenticationData);
    assertEquals(1, testAuthenticator.loadCalls);
    assertEquals(fakeAuthenticationData, testAuthenticator.data);
    assertEquals(fakeAuthenticationData.authMode, testAuthenticator.authMode);

    webUIListenerCallback('close-dialog');
    assertEquals(1, testBrowserProxy.getCallCount('dialogClose'));
  });

  test('AuthenticatorCallbacks', async () => {
    const fakeUrl = 'www.google.com/fake';

    assertTrue(inlineLoginComponent.$.spinner.active);
    testAuthenticator.dispatchEvent(new Event('ready'));
    assertEquals(1, testBrowserProxy.getCallCount('authenticatorReady'));
    assertFalse(inlineLoginComponent.$.spinner.active);

    testAuthenticator.dispatchEvent(
        new CustomEvent('resize', {detail: fakeUrl}));
    const switchToFullTabUrl =
        await testBrowserProxy.whenCalled('switchToFullTab');
    assertEquals(fakeUrl, switchToFullTabUrl);

    const fakeCredentials = {email: 'example@gmail.com'};
    testAuthenticator.dispatchEvent(
        new CustomEvent('authCompleted', {detail: fakeCredentials}));
    const completeLoginResult =
        await testBrowserProxy.whenCalled('completeLogin');
    assertTrue(inlineLoginComponent.$.spinner.active);

    if (isChromeOS &&
        loadTimeData.getBoolean('isArcAccountRestrictionsEnabled')) {
      const expectedCredentials = {
        email: 'example@gmail.com',
        isAvailableInArc: false,
      };
      assertDeepEquals(expectedCredentials, completeLoginResult);
    } else {
      assertEquals(fakeCredentials, completeLoginResult);
    }

    testAuthenticator.dispatchEvent(new Event('showIncognito'));
    assertEquals(1, testBrowserProxy.getCallCount('showIncognito'));

    testAuthenticator.dispatchEvent(new Event('getAccounts'));
    assertEquals(1, testBrowserProxy.getCallCount('getAccounts'));
    await testBrowserProxy.whenCalled('getAccounts').then(function() {
      assertEquals(1, testAuthenticator.getAccountsResponseCalls);
      assertDeepEquals(
          getFakeAccountsList(), testAuthenticator.getAccountsResponseResult);
    });

    testAuthenticator.dispatchEvent(new Event('getDeviceId'));
    assertEquals(1, testBrowserProxy.getCallCount('getDeviceId'));
    await testBrowserProxy.whenCalled('getDeviceId').then(function() {
      assertEquals(1, testAuthenticator.getDeviceIdResponseCalls);
      assertDeepEquals(
          getFakeDeviceId(), testAuthenticator.getDeviceIdResponseResult);
    });
  });

  test('BackButton', () => {
    const backButton =
        inlineLoginComponent.shadowRoot!.querySelector<HTMLElement>(
            '.back-button');
    assertTrue(!!backButton);

    let backInWebviewCalls = 0;
    inlineLoginComponent.$.signinFrame.back = () => backInWebviewCalls++;

    // If we cannot go back in the webview - we should close the dialog.
    inlineLoginComponent.$.signinFrame.canGoBack = () => false;
    backButton.click();
    assertEquals(1, testBrowserProxy.getCallCount('dialogClose'));
    assertEquals(0, backInWebviewCalls);

    testBrowserProxy.reset();

    // Go back in the webview if possible.
    inlineLoginComponent.$.signinFrame.canGoBack = () => true;
    backButton.click();
    assertEquals(0, testBrowserProxy.getCallCount('dialogClose'));
    assertEquals(1, backInWebviewCalls);
  });

  test('OkButton', () => {
    // 'OK' button should be hidden.
    assertTrue(inlineLoginComponent.shadowRoot!
                   .querySelector<HTMLElement>('.next-button')!.hidden);
  });
});
