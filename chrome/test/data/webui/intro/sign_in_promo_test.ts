// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://intro/sign_in_promo.js';

import {IntroBrowserProxyImpl} from 'chrome://intro/browser_proxy.js';
import {SignInPromoElement} from 'chrome://intro/sign_in_promo.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {TestIntroBrowserProxy} from './test_intro_browser_proxy.js';

suite('SignInPromoTest', function() {
  let testElement: SignInPromoElement;
  let testBrowserProxy: TestIntroBrowserProxy;

  setup(function() {
    testBrowserProxy = new TestIntroBrowserProxy();
    IntroBrowserProxyImpl.setInstance(testBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('sign-in-promo');
    document.body.appendChild(testElement);
    return waitBeforeNextRender(testElement);
  });

  teardown(function() {
    testElement.remove();
  });

  test('accept sign-in button callback', function() {
    assertEquals(testBrowserProxy.getCallCount('continueWithAccount'), 0);
    testElement.$.acceptSignInButton.click();
    assertEquals(testBrowserProxy.getCallCount('continueWithAccount'), 1);
  });

  test('decline sign-in button callback', function() {
    assertEquals(testBrowserProxy.getCallCount('continueWithoutAccount'), 0);
    testElement.$.declineSignInButton.click();
    assertEquals(testBrowserProxy.getCallCount('continueWithoutAccount'), 1);
  });
});
