// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://chrome-signin/inline_login_app.js';

import type {InlineLoginAppElement} from 'chrome://chrome-signin/inline_login_app.js';
import {InlineLoginBrowserProxyImpl} from 'chrome://chrome-signin/inline_login_browser_proxy.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {fakeAuthenticationData, TestAuthenticator, TestInlineLoginBrowserProxy} from './inline_login_test_util.js';

suite('InlineLoginTest', () => {
  let inlineLoginComponent: InlineLoginAppElement;
  let testBrowserProxy: TestInlineLoginBrowserProxy;
  let testAuthenticator: TestAuthenticator;

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

    const fakeLstFetchResults = '{result: "fakeLstFetchResults"}';
    webUIListenerCallback('send-lst-fetch-results', fakeLstFetchResults);
    assertEquals(1, testBrowserProxy.getCallCount('lstFetchResults'));
    return testBrowserProxy.whenCalled('lstFetchResults').then(args => {
      assertEquals(fakeLstFetchResults, args);
    });
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

    assertEquals(fakeCredentials, completeLoginResult);

    testAuthenticator.dispatchEvent(new Event('showIncognito'));
    assertEquals(1, testBrowserProxy.getCallCount('showIncognito'));
  });
});
