// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://signin-reauth/signin_reauth_app.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {SigninReauthAppElement} from 'chrome://signin-reauth/signin_reauth_app.js';
import {SigninReauthBrowserProxyImpl} from 'chrome://signin-reauth/signin_reauth_browser_proxy.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSigninReauthBrowserProxy} from './test_signin_reauth_browser_proxy.js';

suite('SigninReauthTest', function() {
  let app: SigninReauthAppElement;
  let browserProxy: TestSigninReauthBrowserProxy;

  setup(function() {
    browserProxy = new TestSigninReauthBrowserProxy();
    SigninReauthBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('signin-reauth-app');
    document.body.append(app);
  });

  function assertDefaultLocale() {
    // This test makes comparisons with strings in their default locale,
    // which is en-US.
    assertEquals(
        'en-US', navigator.language,
        'Cannot verify strings for the ' + navigator.language + 'locale.');
  }

  // Tests that no DCHECKS are thrown during initialization of the UI.
  test('LoadPage', function() {
    assertDefaultLocale();
    assertEquals(
        loadTimeData.getString('signinReauthTitle'),
        app.$.signinReauthTitle.textContent!.trim());
  });

  test('ClickConfirm', function() {
    app.$.confirmButton.click();
    return browserProxy.whenCalled('confirm');
  });

  test('ClickCancel', function() {
    app.$.cancelButton.click();
    return browserProxy.whenCalled('cancel');
  });

  test('ButtonsVisibility', async () => {
    await browserProxy.whenCalled('initialize');
    assertFalse(isVisible(app.$.confirmButton));
    assertFalse(isVisible(app.$.cancelButton));
    assertTrue(!!app.shadowRoot!.querySelector('.spinner'));

    webUIListenerCallback('reauth-type-determined');
    await microtasksFinished();

    assertTrue(isVisible(app.$.confirmButton));
    assertTrue(isVisible(app.$.cancelButton));
    assertFalse(!!app.shadowRoot!.querySelector('.spinner'));

    assertDefaultLocale();
    assertEquals('Yes', app.$.confirmButton.textContent!.trim());
  });
});
