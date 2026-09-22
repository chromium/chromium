// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://intro/sign_in_promo_refresh.js';

import {IntroBrowserProxyImpl as IntroMojoBrowserProxyImpl} from 'chrome://intro/intro_browser_proxy.js';
import {SignInPromoBrowserProxyImpl} from 'chrome://intro/sign_in_promo_browser_proxy.js';
import type {SignInPromoRefreshElement} from 'chrome://intro/sign_in_promo_refresh.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestIntroMojoBrowserProxy} from './test_intro_mojo_browser_proxy.js';
import {TestSignInPromoBrowserProxy} from './test_sign_in_promo_browser_proxy.js';

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
  let testBrowserProxy: TestSignInPromoBrowserProxy;
  let testMojoBrowserProxy: TestIntroMojoBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testBrowserProxy = new TestSignInPromoBrowserProxy();
    testMojoBrowserProxy = new TestIntroMojoBrowserProxy();
    SignInPromoBrowserProxyImpl.setInstance(testBrowserProxy);
    IntroMojoBrowserProxyImpl.setInstance(testMojoBrowserProxy);
    loadTimeData.overrideValues({
      isFirstRunDesktopRevampEnabled: true,
      isDeviceManaged: false,
      disableAnimations: false,
    });
  });

  suite('NonManagedDevice', function() {
    setup(function() {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;

      signInPromoElement = document.createElement('sign-in-promo-refresh');
      document.body.appendChild(signInPromoElement);
      return microtasksFinished();
    });

    test('accept sign-in button clicked', async function() {
      assertSignInButtonsEnabled(signInPromoElement);
      assertEquals(
          0, testBrowserProxy.handler.getCallCount('continueWithAccount'));
      signInPromoElement.$.acceptSignInButton.click();
      await microtasksFinished();
      assertSignInButtonsDisabled(signInPromoElement);
      assertEquals(
          1, testBrowserProxy.handler.getCallCount('continueWithAccount'));
    });

    test('decline sign-in button clicked', async function() {
      assertSignInButtonsEnabled(signInPromoElement);
      assertEquals(
          0, testBrowserProxy.handler.getCallCount('continueWithoutAccount'));
      signInPromoElement.$.declineSignInButton.click();
      await microtasksFinished();
      assertSignInButtonsDisabled(signInPromoElement);
      assertEquals(
          1, testBrowserProxy.handler.getCallCount('continueWithoutAccount'));
    });

    test('"reset-intro-buttons" event resets buttons', async function() {
      assertSignInButtonsEnabled(signInPromoElement);
      signInPromoElement.$.acceptSignInButton.click();
      await microtasksFinished();
      assertSignInButtonsDisabled(signInPromoElement);
      testBrowserProxy.page.onResetButtons();
      await microtasksFinished();
      assertSignInButtonsEnabled(signInPromoElement);
    });
  });

  suite('ManagedDevice', function() {
    setup(function() {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;

      loadTimeData.overrideValues({
        isDeviceManaged: true,
      });

      signInPromoElement = document.createElement('sign-in-promo-refresh');
      document.body.appendChild(signInPromoElement);
      return microtasksFinished();
    });

    test('buttons are disabled if disclaimer is empty', async function() {
      assertSignInButtonsDisabled(signInPromoElement);
      assertEquals('', signInPromoElement.$.disclaimerText.textContent.trim());

      testBrowserProxy.resolveDisclaimer('managedDeviceDisclaimer');
      await microtasksFinished();
      assertEquals(
          'managedDeviceDisclaimer',
          signInPromoElement.$.disclaimerText.textContent.trim());
      assertSignInButtonsEnabled(signInPromoElement);
    });
  });

  test('change animation file depending on the theme', async function() {
    testBrowserProxy.setMatchMediaMatches(false);

    signInPromoElement = document.createElement('sign-in-promo-refresh');
    document.body.appendChild(signInPromoElement);
    await microtasksFinished();

    const leftAnimation = signInPromoElement.$.leftAnimation;
    const rightAnimation = signInPromoElement.$.rightAnimation;
    const bottomAnimation = signInPromoElement.$.bottomAnimation;

    assertTrue(!!leftAnimation);
    assertTrue(leftAnimation.animationUrl.includes('light'));
    assertTrue(!!rightAnimation);
    assertTrue(rightAnimation.animationUrl.includes('light'));
    assertTrue(!!bottomAnimation);
    assertTrue(bottomAnimation.animationUrl.includes('light'));

    testBrowserProxy.setMatchMediaMatches(true);
    await microtasksFinished();

    assertTrue(leftAnimation.animationUrl.includes('dark'));
    assertTrue(rightAnimation.animationUrl.includes('dark'));
    assertTrue(bottomAnimation.animationUrl.includes('dark'));
  });

  test(
      'change animation file depending on the theme with revamp disabled',
      async function() {
        loadTimeData.overrideValues({
          isFirstRunDesktopRevampEnabled: false,
        });

        testBrowserProxy.setMatchMediaMatches(false);

        signInPromoElement = document.createElement('sign-in-promo-refresh');
        document.body.appendChild(signInPromoElement);
        await microtasksFinished();

        const leftAnimation = signInPromoElement.$.leftAnimation;
        const rightAnimation = signInPromoElement.$.rightAnimation;
        const bottomAnimation = signInPromoElement.$.bottomAnimation;

        assertTrue(!!leftAnimation);
        assertTrue(leftAnimation.animationUrl.includes('light_left_static'));
        assertTrue(!!rightAnimation);
        assertTrue(rightAnimation.animationUrl.includes('light_right_static'));
        assertTrue(!!bottomAnimation);
        assertTrue(
            bottomAnimation.animationUrl.includes('light_bottom_static'));

        testBrowserProxy.setMatchMediaMatches(true);
        await microtasksFinished();

        assertTrue(leftAnimation.animationUrl.includes('dark_left'));
        assertFalse(leftAnimation.animationUrl.includes('static'));
        assertTrue(rightAnimation.animationUrl.includes('dark_right'));
        assertFalse(rightAnimation.animationUrl.includes('static'));
        assertTrue(bottomAnimation.animationUrl.includes('dark_bottom'));
        assertFalse(bottomAnimation.animationUrl.includes('static'));
      });

  test('toggles animations', async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    signInPromoElement = document.createElement('sign-in-promo-refresh');
    document.body.appendChild(signInPromoElement);
    await microtasksFinished();

    const leftAnimation = signInPromoElement.$.leftAnimation;
    const rightAnimation = signInPromoElement.$.rightAnimation;
    const bottomAnimation = signInPromoElement.$.bottomAnimation;

    let leftPlay: boolean|null = null;
    leftAnimation.setPlay = (play: boolean) => {
      leftPlay = play;
    };
    let rightPlay: boolean|null = null;
    rightAnimation.setPlay = (play: boolean) => {
      rightPlay = play;
    };
    let bottomPlay: boolean|null = null;
    bottomAnimation.setPlay = (play: boolean) => {
      bottomPlay = play;
    };

    const pageRemote =
        testMojoBrowserProxy.callbackRouter.$.bindNewPipeAndPassRemote();
    pageRemote.toggleAnimations(false);
    await pageRemote.$.flushForTesting();
    await microtasksFinished();

    assertEquals(false, leftPlay);
    assertEquals(false, rightPlay);
    assertEquals(false, bottomPlay);

    pageRemote.toggleAnimations(true);
    await pageRemote.$.flushForTesting();
    await microtasksFinished();

    assertEquals(true, leftPlay);
    assertEquals(true, rightPlay);
    assertEquals(true, bottomPlay);
  });

  test(
      'does not toggle animations when `disableAnimations` is true',
      async function() {
        loadTimeData.overrideValues({
          disableAnimations: true,
        });

        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        signInPromoElement = document.createElement('sign-in-promo-refresh');
        document.body.appendChild(signInPromoElement);
        await microtasksFinished();

        const leftAnimation = signInPromoElement.$.leftAnimation;

        let leftPlay: boolean|null = null;
        leftAnimation.setPlay = (play: boolean) => {
          leftPlay = play;
        };

        const pageRemote =
            testMojoBrowserProxy.callbackRouter.$.bindNewPipeAndPassRemote();
        pageRemote.toggleAnimations(false);
        await pageRemote.$.flushForTesting();
        await microtasksFinished();

        // Verify that setPlay was NOT called because animations are disabled.
        assertEquals(null, leftPlay);
      });
});
