// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://chrome-signin/inline_login_app.js';

import {AccountAdditionOptions} from 'chrome://chrome-signin/arc_account_picker/arc_util.js';
import {InlineLoginBrowserProxyImpl} from 'chrome://chrome-signin/inline_login_browser_proxy.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {isChromeOS, webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {waitAfterNextRender} from '../test_util.js';

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
  IsAvailableInArc: 'IsAvailableInArc',
  ToggleHidden: 'ToggleHidden',
  LinkClick: 'LinkClick',
};

suite(inline_login_welcome_page_test.suiteName, () => {
  /** @type {InlineLoginAppElement} */
  let inlineLoginComponent;
  /** @type {TestInlineLoginBrowserProxy} */
  let testBrowserProxy;
  /** @type {TestAuthenticator} */
  let testAuthenticator;

  /**
   * @return {string} id of the active view.
   */
  function getActiveViewId() {
    return inlineLoginComponent.$$('div.active[slot="view"]').id;
  }

  /**
   * @param {?AccountAdditionOptions} dialogArgs
   */
  function testSetup(dialogArgs) {
    document.body.innerHTML = '';
    testBrowserProxy = new TestInlineLoginBrowserProxy();
    testBrowserProxy.setDialogArguments(dialogArgs);
    InlineLoginBrowserProxyImpl.instance_ = testBrowserProxy;
    document.body.innerHTML = '';
    inlineLoginComponent = /** @type {InlineLoginAppElement} */ (
        document.createElement('inline-login-app'));
    document.body.appendChild(inlineLoginComponent);
    testAuthenticator = new TestAuthenticator();
    inlineLoginComponent.setAuthExtHostForTest(testAuthenticator);
    flush();
  }

  suiteSetup(function() {
    loadTimeData.overrideValues({shouldSkipWelcomePage: false});
  });

  test(
      assert(inline_login_welcome_page_test.TestNames.Reauthentication), () => {
        testSetup(/*dialogArgs=*/ null);
        webUIListenerCallback(
            'load-auth-extension', fakeAuthExtensionDataWithEmail);
        // Welcome screen should be skipped for reauth.
        assertEquals(
            inlineLoginComponent.View.addAccount, getActiveViewId(),
            'Welcome screen should be active');
      });

  test(assert(inline_login_welcome_page_test.TestNames.OkButton), () => {
    testSetup(/*dialogArgs=*/ null);
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

    if (loadTimeData.getBoolean('isArcAccountRestrictionsEnabled')) {
      return;
    }

    return testBrowserProxy.whenCalled('skipWelcomePage').then(skip => {
      assertEquals(
          false, skip, 'skipWelcomePage should be called with "false"');
    });
  });

  test(assert(inline_login_welcome_page_test.TestNames.Checkbox), () => {
    testSetup(/*dialogArgs=*/ null);

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
    testSetup(/*dialogArgs=*/ null);
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

  test(
      assert(inline_login_welcome_page_test.TestNames.IsAvailableInArc), () => {
        const dialogArgs = {
          isAvailableInArc: true,
          showArcAvailabilityPicker: false
        };
        testSetup(dialogArgs);
        const toggle = inlineLoginComponent.$$('welcome-page-app')
                           .$$('.arc-toggle-container');
        assertTrue(!!toggle);
        assertFalse(toggle.hidden, 'ARC toggle should be visible');
        const toggleButton =
            inlineLoginComponent.$$('welcome-page-app').$$('cr-toggle');
        assertTrue(!!toggleButton);
        assertTrue(toggleButton.checked);
        toggleButton.click();
        flush();
        return waitAfterNextRender(toggleButton).then(() => {
          assertFalse(toggleButton.checked);
        });
      });

  test(assert(inline_login_welcome_page_test.TestNames.ToggleHidden), () => {
    const dialogArgs = {
      isAvailableInArc: true,
      showArcAvailabilityPicker: true
    };
    testSetup(dialogArgs);
    const toggle =
        inlineLoginComponent.$$('welcome-page-app').$$('.arc-toggle-container');
    assertTrue(!!toggle);
    assertTrue(toggle.hidden, 'ARC toggle should be hidden');
  });

  test(assert(inline_login_welcome_page_test.TestNames.LinkClick), async () => {
    const dialogArgs = {
      isAvailableInArc: true,
      showArcAvailabilityPicker: false
    };
    testSetup(dialogArgs);

    inlineLoginComponent.$$('welcome-page-app').$$('#osSettingsLink').click();
    await testBrowserProxy.whenCalled('dialogClose');

    inlineLoginComponent.$$('welcome-page-app').$$('#appsSettingsLink').click();
    await testBrowserProxy.whenCalled('dialogClose');

    inlineLoginComponent.$$('welcome-page-app').$$('#newPersonLink').click();
    await testBrowserProxy.whenCalled('dialogClose');

    inlineLoginComponent.$$('welcome-page-app').$$('#guestModeLink').click();
    return testBrowserProxy.whenCalled('openGuestWindow');
  });
});
