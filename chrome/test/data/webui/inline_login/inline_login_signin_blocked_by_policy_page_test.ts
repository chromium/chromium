// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for the Inline login flow ('Add account' flow) on
 * Chrome OS when a secondary account is being added and the sign-in should be
 * blocked by the policy SecondaryGoogleAccountUsage.
 */

import 'chrome://chrome-signin/inline_login_app.js';

import {AccountAdditionOptions} from 'chrome://chrome-signin/arc_account_picker/arc_util.js';
import {InlineLoginAppElement, View} from 'chrome://chrome-signin/inline_login_app.js';
import {InlineLoginBrowserProxyImpl} from 'chrome://chrome-signin/inline_login_browser_proxy.js';
import {SigninBlockedByPolicyPageElement} from 'chrome://chrome-signin/signin_blocked_by_policy_page.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {fakeSigninBlockedByPolicyData, TestAuthenticator, TestInlineLoginBrowserProxy} from './inline_login_test_util.js';

suite('InlineLoginSigninBlockedByPolicyPageTest', () => {
  let signinBlockedByPolicyPageComponent: SigninBlockedByPolicyPageElement;
  let inlineLoginComponent: InlineLoginAppElement;
  let testBrowserProxy: TestInlineLoginBrowserProxy;
  let testAuthenticator: TestAuthenticator;

  /** @return id of the active view. */
  function getActiveViewId(): string {
    return inlineLoginComponent.shadowRoot!
        .querySelector('div.active[slot="view"]')!.id;
  }

  function testSetup(dialogArgs: AccountAdditionOptions|null) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testBrowserProxy = new TestInlineLoginBrowserProxy();
    testBrowserProxy.setDialogArguments(dialogArgs);
    InlineLoginBrowserProxyImpl.setInstance(testBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    inlineLoginComponent = /** @type {InlineLoginAppElement} */ (
        document.createElement('inline-login-app'));
    document.body.appendChild(inlineLoginComponent);
    testAuthenticator = new TestAuthenticator();
    inlineLoginComponent.setAuthenticatorForTest(testAuthenticator);
    flush();
    signinBlockedByPolicyPageComponent =
        inlineLoginComponent.shadowRoot!.querySelector(
            'signin-blocked-by-policy-page')!;
  }

  test('BlockedSigninPage', () => {
    testSetup(/*dialogArgs=*/ null);
    // Fire web UI listener to switch the ui view to
    // `signinBlockedByPolicy`.
    webUIListenerCallback(
        'show-signin-error-page', fakeSigninBlockedByPolicyData);
    assertEquals(
        View.SIGNIN_BLOCKED_BY_POLICY, getActiveViewId(),
        'Sing-in blocked by policy page should be shown');

    const title =
        signinBlockedByPolicyPageComponent.shadowRoot!.querySelector('h1');
    assertTrue(!!title);
    assertEquals(
        title.textContent, 'Can\'t sign in with this account',
        'The titles do not match');
    const textBody = signinBlockedByPolicyPageComponent.shadowRoot!
                         .querySelector<HTMLElement>('.secondary');
    assertTrue(!!textBody);
    assertEquals(
        textBody.textContent!,
        'john.doe@example.com is managed by example.com. You can\'t add ' +
            'this email as an additional account.\n    To use ' +
            'john.doe@example.com, first sign out of your Chromebook. ' +
            'Then at the bottom of the login screen, select Add Person.',
        'The text bodies do not match');
  });

  test('FireWebUIListenerCallback', () => {
    testSetup(/*dialogArgs=*/ null);
    // Fire web UI listener to switch the ui view to
    // `signinBlockedByPolicy`.
    webUIListenerCallback(
        'show-signin-error-page', fakeSigninBlockedByPolicyData);
    assertEquals(
        View.SIGNIN_BLOCKED_BY_POLICY, getActiveViewId(),
        'Sing-in blocked by policy should be shown');
    let textBody = signinBlockedByPolicyPageComponent.shadowRoot!
                       .querySelector<HTMLElement>('.secondary');
    assertTrue(!!textBody);
    assertTrue(
        textBody.textContent!.includes('john.doe@example.com'),
        'Invalid user email');
    assertTrue(
        textBody.textContent!.includes('example.com'), 'Invalid hosted domain');
    assertTrue(
        textBody.textContent!.includes('Chromebook'), 'Invalid device type');
    webUIListenerCallback('show-signin-error-page', {
      email: 'coyote@acme.com',
      hostedDomain: 'acme.com',
      deviceType: 'Chromebox',
      signinBlockedByPolicy: true,
    });
    textBody = signinBlockedByPolicyPageComponent.shadowRoot!
                   .querySelector<HTMLElement>('.secondary');
    assertTrue(!!textBody);
    assertTrue(
        textBody.textContent!.includes('coyote@acme.com'),
        'Invalid user email');
    assertTrue(
        textBody.textContent!.includes('acme.com'), 'Invalid hosted domain');
    assertTrue(
        textBody.textContent!.includes('Chromebox'), 'Invalid device type');
  });

  test('OkButton', async () => {
    testSetup(/*dialogArgs=*/ null);
    // Fire web UI listener to switch the ui view to
    // `signinBlockedByPolicy`.
    webUIListenerCallback(
        'show-signin-error-page', fakeSigninBlockedByPolicyData);

    const okButton =
        inlineLoginComponent.shadowRoot!.querySelector<HTMLElement>(
            '.next-button');
    assertTrue(!!okButton);
    // OK button and signin blocked by policy screen should be visible.
    assertFalse(okButton.hidden, 'OK button should be visible');
    assertEquals(
        View.SIGNIN_BLOCKED_BY_POLICY, getActiveViewId(),
        'Sing-in blocked by policy should be shown');

    okButton.click();
    await testBrowserProxy.whenCalled('dialogClose');
  });
});
