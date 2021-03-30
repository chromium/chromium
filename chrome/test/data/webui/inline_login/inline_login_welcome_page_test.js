// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://chrome-signin/inline_login_app.js';

import {InlineLoginBrowserProxyImpl} from 'chrome://chrome-signin/inline_login_browser_proxy.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {isChromeOS} from 'chrome://resources/js/cr.m.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';

import {fakeAuthExtensionData, fakeAuthExtensionDataWithEmail, TestAuthenticator, TestInlineLoginBrowserProxy} from './inline_login_test_util.js';

window.inline_login_welcome_page_test = {};
const inline_login_welcome_page_test = window.inline_login_welcome_page_test;
inline_login_welcome_page_test.suiteName = 'InlineLoginWelcomePageTest';

/** @enum {string} */
inline_login_welcome_page_test.TestNames = {
  Reauthentication: 'Reauthentication',
  OkButton: 'OkButton',
  Checkbox: 'Checkbox',
  GoBack: 'GoBack',
};

suite(inline_login_welcome_page_test.suiteName, () => {
  /** @type {InlineLoginAppElement} */
  let inlineLoginComponent;
  /** @type {TestInlineLoginBrowserProxy} */
  let testBrowserProxy;
  /** @type {TestAuthenticator} */
  let testAuthenticator;

  function getActiveViewId() {
    return inlineLoginComponent.$$('div.active[slot="view"]').id;
  }

  suiteSetup(function() {
    // This is ChromeOS-only test.
    assertTrue(isChromeOS);
    loadTimeData.overrideValues({shouldSkipWelcomePage: false});
  });

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

  test(
      assert(inline_login_welcome_page_test.TestNames.Reauthentication), () => {
        webUIListenerCallback(
            'load-auth-extension', fakeAuthExtensionDataWithEmail);
        // Welcome screen should be skipped for reauth.
        assertEquals(
            inlineLoginComponent.View.addAccount, getActiveViewId(),
            'Welcome screen should be active');
      });

  test(assert(inline_login_welcome_page_test.TestNames.OkButton), () => {
    webUIListenerCallback('load-auth-extension', fakeAuthExtensionData);
    const okButton = inlineLoginComponent.$$('.next-button');
    // OK button and welcome screen should be visible.
    assertFalse(okButton.hidden, 'OK button should be visible');
    assertEquals(
        inlineLoginComponent.View.welcome, getActiveViewId(),
        'Welcome screen should be active');

    okButton.click();
    assertEquals(
        inlineLoginComponent.View.addAccount, getActiveViewId(),
        'Add account screen should be active');
    return testBrowserProxy.whenCalled('skipWelcomePage').then(skip => {
      assertEquals(
          false, skip, 'skipWelcomePage should be called with "false"');
    });
  });

  test(assert(inline_login_welcome_page_test.TestNames.Checkbox), () => {
    webUIListenerCallback('load-auth-extension', fakeAuthExtensionData);
    const checkbox =
        inlineLoginComponent.$$('welcome-page-app').$$('cr-checkbox');
    assertFalse(checkbox.checked, 'Checkbox should be unchecked by default');
    checkbox.click();
    assertTrue(checkbox.checked, 'Checkbox should be checked');

    const okButton = inlineLoginComponent.$$('.next-button');
    okButton.click();
    return testBrowserProxy.whenCalled('skipWelcomePage').then(skip => {
      assertEquals(true, skip, 'skipWelcomePage should be called with "true"');
    });
  });

  test(assert(inline_login_welcome_page_test.TestNames.GoBack), () => {
    webUIListenerCallback('load-auth-extension', fakeAuthExtensionData);
    const backButton = inlineLoginComponent.$$('.back-button');
    const okButton = inlineLoginComponent.$$('.next-button');

    assertTrue(backButton.hidden, 'Back button should be hidden');
    assertFalse(okButton.hidden, 'OK button should be visible');
    assertEquals(
        inlineLoginComponent.View.welcome, getActiveViewId(),
        'Welcome screen should be active');

    okButton.click();
    assertTrue(okButton.hidden, 'OK button should be hidden');
    assertFalse(backButton.hidden, 'Back button should be visible');
    assertEquals(
        inlineLoginComponent.View.addAccount, getActiveViewId(),
        'Add account screen should be active');

    backButton.click();
    assertTrue(backButton.hidden, 'Back button should be hidden');
    assertFalse(okButton.hidden, 'OK button should be visible');
    assertEquals(
        inlineLoginComponent.View.welcome, getActiveViewId(),
        'Welcome screen should be active');
  });
});
