// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://intro/sign_in_promo_refresh.js';

import {IntroBrowserProxyImpl} from 'chrome://intro/browser_proxy.js';
import type {SignInPromoRefreshElement} from 'chrome://intro/sign_in_promo_refresh.js';
import {Variation} from 'chrome://intro/sign_in_promo_refresh.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestIntroBrowserProxy} from './test_intro_browser_proxy.js';

function assertSignInButtonsDisabled(
    element: SignInPromoRefreshElement, assertDeclineButton: boolean = true) {
  assertTrue(element.$.acceptSignInButton.disabled);
  if (assertDeclineButton) {
    assertTrue(element.$.declineSignInButton.disabled);
  }
}

function assertSignInButtonsEnabled(
    element: SignInPromoRefreshElement, assertDeclineButton: boolean = true) {
  assertFalse(element.$.acceptSignInButton.disabled);
  if (assertDeclineButton) {
    assertFalse(element.$.declineSignInButton.disabled);
  }
}

function variationToTestSuffix(variation: Variation): string {
  switch (variation) {
    case Variation.DEFAULT:
      return 'Default';
    case Variation.DONT_SIGN_IN_IN_TOP_RIGHT_CORNER:
      return 'DontSignInInTopRightCorner';
    case Variation.DONT_SIGN_IN_ON_GAIA:
      return 'DontSignInOnGaia';
    default:
      throw new Error('Unknown variation');
  }
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

  [Variation.DEFAULT, Variation.DONT_SIGN_IN_IN_TOP_RIGHT_CORNER,
   Variation.DONT_SIGN_IN_ON_GAIA]
      .forEach((variation) => {
        const assertDeclineButton =
            variation !== Variation.DONT_SIGN_IN_ON_GAIA;

        suite(
            'NonManagedDevice' + variationToTestSuffix(variation), function() {
              setup(function() {
                loadTimeData.overrideValues({
                  isDeviceManaged: false,
                  signInPromoVariation: variation,
                });

                document.body.innerHTML = window.trustedTypes!.emptyHTML;
                signInPromoElement =
                    document.createElement('sign-in-promo-refresh');
                document.body.appendChild(signInPromoElement);
                return microtasksFinished();
              });

              test('accept sign-in button clicked', async function() {
                assertSignInButtonsEnabled(
                    signInPromoElement, assertDeclineButton);
                assertEquals(
                    0, testBrowserProxy.getCallCount('continueWithAccount'));
                signInPromoElement.$.acceptSignInButton.click();
                await microtasksFinished();
                assertSignInButtonsDisabled(
                    signInPromoElement, assertDeclineButton);
                assertEquals(
                    1, testBrowserProxy.getCallCount('continueWithAccount'));
              });

              test('decline sign-in button clicked', async function() {
                if (!assertDeclineButton) {
                  this.skip();
                }
                assertSignInButtonsEnabled(signInPromoElement);
                assertEquals(
                    0, testBrowserProxy.getCallCount('continueWithoutAccount'));
                signInPromoElement.$.declineSignInButton.click();
                await microtasksFinished();
                assertSignInButtonsDisabled(signInPromoElement);
                assertEquals(
                    1, testBrowserProxy.getCallCount('continueWithoutAccount'));
              });

              test(
                  '"reset-intro-buttons" event resets buttons',
                  async function() {
                    assertSignInButtonsEnabled(
                        signInPromoElement, assertDeclineButton);
                    signInPromoElement.$.acceptSignInButton.click();
                    await microtasksFinished();
                    assertSignInButtonsDisabled(
                        signInPromoElement, assertDeclineButton);
                    webUIListenerCallback('reset-intro-buttons');
                    await microtasksFinished();
                    assertSignInButtonsEnabled(
                        signInPromoElement, assertDeclineButton);
                  });
            });
      });

  [Variation.DEFAULT, Variation.DONT_SIGN_IN_IN_TOP_RIGHT_CORNER,
   Variation.DONT_SIGN_IN_ON_GAIA]
      .forEach((variation) => {
        const assertDeclineButton =
            variation !== Variation.DONT_SIGN_IN_ON_GAIA;

        suite('ManagedDevice' + variationToTestSuffix(variation), function() {
          setup(function() {
            loadTimeData.overrideValues({
              isDeviceManaged: true,
              signInPromoVariation: variation,
            });

            document.body.innerHTML = window.trustedTypes!.emptyHTML;
            signInPromoElement =
                document.createElement('sign-in-promo-refresh');
            document.body.appendChild(signInPromoElement);
            return microtasksFinished();
          });

          test('buttons are disabled if disclaimer is empty', async function() {
            assertSignInButtonsDisabled(
                signInPromoElement, assertDeclineButton);
            assertEquals(
                '', signInPromoElement.$.disclaimerText.textContent.trim());

            webUIListenerCallback(
                'managed-device-disclaimer-updated', 'managedDeviceDisclaimer');
            await microtasksFinished();
            assertEquals(
                'managedDeviceDisclaimer',
                signInPromoElement.$.disclaimerText.textContent.trim());
            assertSignInButtonsEnabled(signInPromoElement, assertDeclineButton);
          });
        });
      });

  test('default promo variation', async function() {
    loadTimeData.overrideValues({
      isDeviceManaged: false,
      signInPromoVariation: Variation.DEFAULT,
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    signInPromoElement = document.createElement('sign-in-promo-refresh');
    document.body.appendChild(signInPromoElement);
    await microtasksFinished();

    const createAccountDisclaimer = signInPromoElement.shadowRoot.querySelector(
        '#create-account-disclaimer');
    assertFalse(!!createAccountDisclaimer);

    const topRightCornerContainer = signInPromoElement.shadowRoot.querySelector(
        '#top-right-corner-container');
    assertFalse(!!topRightCornerContainer);

    const buttonContainer =
        signInPromoElement.shadowRoot.querySelector('#buttonContainer');
    assertTrue(!!buttonContainer);
    assertEquals(
        signInPromoElement.$.declineSignInButton,
        buttonContainer.querySelector('#declineSignInButton'));
  });

  test('don\'t sign in in top right corner promo variation', async function() {
    loadTimeData.overrideValues({
      isDeviceManaged: false,
      signInPromoVariation: Variation.DONT_SIGN_IN_IN_TOP_RIGHT_CORNER,
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    signInPromoElement = document.createElement('sign-in-promo-refresh');
    document.body.appendChild(signInPromoElement);
    await microtasksFinished();

    const createAccountDisclaimer = signInPromoElement.shadowRoot.querySelector(
        '#create-account-disclaimer');
    assertTrue(!!createAccountDisclaimer);

    const topRightCornerContainer = signInPromoElement.shadowRoot.querySelector(
        '#top-right-corner-container');
    assertTrue(!!topRightCornerContainer);
    assertEquals(
        signInPromoElement.$.declineSignInButton,
        topRightCornerContainer.querySelector('#declineSignInButton'));

    const buttonContainer =
        signInPromoElement.shadowRoot.querySelector('#buttonContainer');
    assertTrue(!!buttonContainer);
    const declineSignInButtonInButtonContainer =
        buttonContainer.querySelector('#declineSignInButton');
    assertFalse(!!declineSignInButtonInButtonContainer);
  });

  test('don\'t sign in on Gaia page promo variation', async function() {
    loadTimeData.overrideValues({
      isDeviceManaged: false,
      signInPromoVariation: Variation.DONT_SIGN_IN_ON_GAIA,
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    signInPromoElement = document.createElement('sign-in-promo-refresh');
    document.body.appendChild(signInPromoElement);
    await microtasksFinished();

    const createAccountDisclaimer = signInPromoElement.shadowRoot.querySelector(
        '#create-account-disclaimer');
    assertFalse(!!createAccountDisclaimer);

    const declineSignInButton =
        signInPromoElement.shadowRoot.querySelector('#declineSignInButton');
    assertFalse(!!declineSignInButton);
  });
});
