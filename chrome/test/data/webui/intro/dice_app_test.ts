// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://intro/dice_app.js';

import {IntroBrowserProxyImpl} from 'chrome://intro/browser_proxy.js';
import {IntroAppElement} from 'chrome://intro/dice_app.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {TestIntroBrowserProxy} from './test_intro_browser_proxy.js';

suite('DiceAppTest', function() {
  let testElement: IntroAppElement;
  let testBrowserProxy: TestIntroBrowserProxy;

  setup(function() {
    testBrowserProxy = new TestIntroBrowserProxy();
    IntroBrowserProxyImpl.setInstance(testBrowserProxy);

    document.body.innerHTML =
        window.trustedTypes!.emptyHTML as unknown as string;
    testElement = document.createElement('intro-app');
    document.body.appendChild(testElement);
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