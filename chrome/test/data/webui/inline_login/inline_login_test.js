// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://chrome-signin/inline_login_app.js';

import {InlineLoginBrowserProxyImpl} from 'chrome://chrome-signin/inline_login_browser_proxy.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {isChromeOS} from 'chrome://resources/js/cr.m.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {getFakeAccountsList, TestAuthenticator, TestInlineLoginBrowserProxy} from './inline_login_test_util.js';

window.inline_login_test = {};
const inline_login_test = window.inline_login_test;
inline_login_test.suiteName = 'InlineLoginTest';

/** @enum {string} */
inline_login_test.TestNames = {
  Initialize: 'Initialize',
  WebUICallbacks: 'WebUICallbacks',
  AuthExtHostCallbacks: 'AuthExtHostCallbacks',
  BackButton: 'BackButton'
};

suite(inline_login_test.suiteName, () => {
  /** @type {InlineLoginAppElement} */
  let inlineLoginComponent;
  /** @type {TestInlineLoginBrowserProxy} */
  let testBrowserProxy;
  /** @type {TestAuthenticator} */
  let testAuthenticator;

  setup(() => {
    document.body.innerHTML = '';
    testBrowserProxy = new TestInlineLoginBrowserProxy();
    InlineLoginBrowserProxyImpl.instance_ = testBrowserProxy;
    document.body.innerHTML = '';
    inlineLoginComponent = /** @type {InlineLoginAppElement} */ (
        document.createElement('inline-login-app'));
    document.body.appendChild(inlineLoginComponent);
    testAuthenticator = new TestAuthenticator();
    inlineLoginComponent.setAuthExtHostForTest(testAuthenticator);
    flush();
  });

  test(assert(inline_login_test.TestNames.Initialize), () => {
    // The 'loading' spinner should be shown and 'initialize' should be called
    // on startup.
    assertTrue(inlineLoginComponent.$$('paper-spinner-lite').active);
    assertTrue(inlineLoginComponent.$$('#signinFrame').hidden);
    assertEquals(1, testBrowserProxy.getCallCount('initialize'));
  });

  test(assert(inline_login_test.TestNames.WebUICallbacks), () => {
    const fakeAuthExtensionData = {
      hl: 'hl',
      gaiaUrl: 'gaiaUrl',
      authMode: 1,
      email: 'example@gmail.com',
    };
    webUIListenerCallback('load-auth-extension', fakeAuthExtensionData);
    assertEquals(1, testAuthenticator.loadCalls);
    assertEquals(fakeAuthExtensionData, testAuthenticator.data);
    assertEquals(fakeAuthExtensionData.authMode, testAuthenticator.authMode);

    webUIListenerCallback('close-dialog');
    assertEquals(1, testBrowserProxy.getCallCount('dialogClose'));

    const fakeLstFetchResults = '{result: "fakeLstFetchResults"}';
    webUIListenerCallback('send-lst-fetch-results', fakeLstFetchResults);
    assertEquals(1, testBrowserProxy.getCallCount('lstFetchResults'));
    return testBrowserProxy.whenCalled('lstFetchResults').then(args => {
      assertEquals(fakeLstFetchResults, args);
    });
  });

  test(assert(inline_login_test.TestNames.AuthExtHostCallbacks), async () => {
    const fakeUrl = 'www.google.com/fake';

    assertTrue(inlineLoginComponent.$$('paper-spinner-lite').active);
    testAuthenticator.dispatchEvent(new Event('ready'));
    assertEquals(1, testBrowserProxy.getCallCount('authExtensionReady'));
    assertFalse(inlineLoginComponent.$$('paper-spinner-lite').active);

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
    assertTrue(inlineLoginComponent.$$('paper-spinner-lite').active);
    assertEquals(fakeCredentials, completeLoginResult);

    testAuthenticator.dispatchEvent(new Event('showIncognito'));
    assertEquals(1, testBrowserProxy.getCallCount('showIncognito'));

    testAuthenticator.dispatchEvent(new Event('getAccounts'));
    assertEquals(1, testBrowserProxy.getCallCount('getAccounts'));
    return testBrowserProxy.whenCalled('getAccounts').then(function() {
      assertEquals(1, testAuthenticator.getAccountsResponseCalls);
      assertDeepEquals(
          getFakeAccountsList(), testAuthenticator.getAccountsResponseResult);
    });
  });

  test(assert(inline_login_test.TestNames.BackButton), () => {
    const backButton = inlineLoginComponent.$$('.back-button');
    if (!isChromeOS) {
      // Back button should be shown only on ChromeOS.
      assertEquals(null, backButton);
      return;
    }

    let backInWebviewCalls = 0;
    inlineLoginComponent.$$('#signinFrame').back = () => backInWebviewCalls++;

    // If we cannot go back in the webview - we should close the dialog.
    inlineLoginComponent.$$('#signinFrame').canGoBack = () => false;
    backButton.click();
    assertEquals(1, testBrowserProxy.getCallCount('dialogClose'));
    assertEquals(0, backInWebviewCalls);

    testBrowserProxy.reset();

    // Go back in the webview if possible.
    inlineLoginComponent.$$('#signinFrame').canGoBack = () => true;
    backButton.click();
    assertEquals(0, testBrowserProxy.getCallCount('dialogClose'));
    assertEquals(1, backInWebviewCalls);
  });
});
