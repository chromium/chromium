// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://intro/sign_in_promo_refresh.js';

import {IntroBrowserProxyImpl} from 'chrome://intro/browser_proxy.js';
import type {SignInPromoRefreshElement} from 'chrome://intro/sign_in_promo_refresh.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestIntroBrowserProxy} from './test_intro_browser_proxy.js';

function assertSignInButtonsDisabled(element: SignInPromoRefreshElement) {
  assertTrue(element.$.acceptSignInButton.disabled);
  assertTrue(element.$.declineSignInButton.disabled);
}

function assertSignInButtonsEnabled(element: SignInPromoRefreshElement) {
  assertFalse(element.$.acceptSignInButton.disabled);
  assertFalse(element.$.declineSignInButton.disabled);
}

suite('SignInPromoRefreshTest', function() {
  let signInPromoElement: SignInPromoRefreshElement;
  let testBrowserProxy: TestIntroBrowserProxy;

  setup(function() {
    testBrowserProxy = new TestIntroBrowserProxy();
    IntroBrowserProxyImpl.setInstance(testBrowserProxy);
  });

  teardown(function() {
    signInPromoElement.remove();
  });

  suite('NonManagedDevice', function() {
    setup(function() {
      loadTimeData.overrideValues({
        isDeviceManaged: false,
      });

      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      signInPromoElement = document.createElement('sign-in-promo-refresh');
      document.body.appendChild(signInPromoElement);
      return microtasksFinished();
    });

    test('accept sign-in button clicked', async function() {
      assertSignInButtonsEnabled(signInPromoElement);
      assertEquals(0, testBrowserProxy.getCallCount('continueWithAccount'));
      signInPromoElement.$.acceptSignInButton.click();
      await microtasksFinished();
      assertSignInButtonsDisabled(signInPromoElement);
      assertEquals(1, testBrowserProxy.getCallCount('continueWithAccount'));
    });

    test('decline sign-in button clicked', async function() {
      assertSignInButtonsEnabled(signInPromoElement);
      assertEquals(0, testBrowserProxy.getCallCount('continueWithoutAccount'));
      signInPromoElement.$.declineSignInButton.click();
      await microtasksFinished();
      assertSignInButtonsDisabled(signInPromoElement);
      assertEquals(1, testBrowserProxy.getCallCount('continueWithoutAccount'));
    });

    test('"reset-intro-buttons" event resets buttons', async function() {
      assertSignInButtonsEnabled(signInPromoElement);
      signInPromoElement.$.acceptSignInButton.click();
      await microtasksFinished();
      assertSignInButtonsDisabled(signInPromoElement);
      webUIListenerCallback('reset-intro-buttons');
      await microtasksFinished();
      assertSignInButtonsEnabled(signInPromoElement);
    });
  });

  suite('ManagedDevice', function() {
    setup(function() {
      loadTimeData.overrideValues({
        isDeviceManaged: true,
      });

      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      signInPromoElement = document.createElement('sign-in-promo-refresh');
      document.body.appendChild(signInPromoElement);
      return microtasksFinished();
    });

    test('buttons are disabled if disclaimer is empty', async function() {
      assertSignInButtonsDisabled(signInPromoElement);
      assertEquals('', signInPromoElement.$.disclaimerText.textContent.trim());

      webUIListenerCallback(
          'managed-device-disclaimer-updated', 'managedDeviceDisclaimer');
      await microtasksFinished();
      assertEquals(
          'managedDeviceDisclaimer',
          signInPromoElement.$.disclaimerText.textContent.trim());
      assertSignInButtonsEnabled(signInPromoElement);
    });
  });
});
