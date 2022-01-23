// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://chrome-signin/inline_login_app.js';

import {InlineLoginBrowserProxyImpl} from 'chrome://chrome-signin/inline_login_browser_proxy.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals} from '../chai_assert.js';

import {fakeAuthExtensionData, fakeAuthExtensionDataWithEmail, TestAuthenticator, TestInlineLoginBrowserProxy} from './inline_login_test_util.js';

window.arc_account_picker_page_test = {};
const arc_account_picker_page_test = window.arc_account_picker_page_test;
arc_account_picker_page_test.suiteName = 'InlineLoginWelcomePageTest';

/** @enum {string} */
arc_account_picker_page_test.TestNames = {
  ArcPickerActive: 'ArcPickerActive',
  ArcPickerHiddenForReauth: 'ArcPickerHiddenForReauth',
};

suite(arc_account_picker_page_test.suiteName, () => {
  /** @type {InlineLoginAppElement} */
  let inlineLoginComponent;
  /** @type {TestInlineLoginBrowserProxy} */
  let testBrowserProxy;

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
    inlineLoginComponent = /** @type {InlineLoginAppElement} */ (
        document.createElement('inline-login-app'));
    document.body.appendChild(inlineLoginComponent);
    inlineLoginComponent.setAuthExtHostForTest(new TestAuthenticator());
    flush();
  }

  test(assert(arc_account_picker_page_test.TestNames.ArcPickerActive), () => {
    testSetup({isAvailableInArc: true, showArcAvailabilityPicker: true});
    webUIListenerCallback('load-auth-extension', fakeAuthExtensionData);
    assertEquals(
        inlineLoginComponent.View.arcAccountPicker, getActiveViewId(),
        'ARC account picker screen should be active');
  });

  test(
      assert(arc_account_picker_page_test.TestNames.ArcPickerHiddenForReauth),
      () => {
        testSetup({isAvailableInArc: true, showArcAvailabilityPicker: true});
        webUIListenerCallback(
            'load-auth-extension', fakeAuthExtensionDataWithEmail);
        assertEquals(
            inlineLoginComponent.View.addAccount, getActiveViewId(),
            'Add account view should be active for reauthentication');
      });
});
