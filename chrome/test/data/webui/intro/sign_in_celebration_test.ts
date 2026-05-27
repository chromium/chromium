// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://intro/sign_in_celebration/app.js';

import {SignInCelebrationPageCallbackRouter, SignInCelebrationPageHandlerRemote} from 'chrome://intro/sign_in_celebration.mojom-webui.js';
import type {SignInCelebrationPageRemote} from 'chrome://intro/sign_in_celebration.mojom-webui.js';
import {SignInCelebrationBrowserProxyImpl} from 'chrome://intro/sign_in_celebration/sign_in_celebration_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('SignInCelebrationTest', function() {
  let handler: TestMock<SignInCelebrationPageHandlerRemote>&
      SignInCelebrationPageHandlerRemote;
  let page: SignInCelebrationPageRemote;

  const userInfo = {
    avatarUrl: 'http://example.com/image.png',
    title: 'Welcome, Test User',
  };

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    const callbackRouter = new SignInCelebrationPageCallbackRouter();
    handler = TestMock.fromClass(SignInCelebrationPageHandlerRemote);
    page = callbackRouter.$.bindNewPipeAndPassRemote();

    SignInCelebrationBrowserProxyImpl.setInstance({
      callbackRouter: callbackRouter,
      handler: handler,
      matchMedia: (query: string) => window.matchMedia(query),
    });

    handler.setPromiseResolveFor(
        'getSignInCelebrationUserInfo', {userInfo: userInfo});

    loadTimeData.overrideValues({disableAnimations: false});
  });

  async function createSignInCelebrationAppElement() {
    const element = document.createElement('sign-in-celebration-app');
    document.body.appendChild(element);
    await microtasksFinished();
    return element;
  }

  test('SetsInitialUserInfo', async function() {
    const testElement = await createSignInCelebrationAppElement();
    await handler.whenCalled('getSignInCelebrationUserInfo');
    await microtasksFinished();

    assertEquals(userInfo.title, testElement.$.title.textContent);
    assertEquals(userInfo.avatarUrl, testElement.$.avatar.src);
  });

  test('UpdatesUserInfo', async function() {
    const testElement = await createSignInCelebrationAppElement();
    const newUserInfo = {
      avatarUrl: 'chrome://theme/IDR_PROFILE_AVATAR_1',
      title: 'Welcome, Updated User',
    };

    page.onSignInCelebrationUserInfoUpdated(newUserInfo);
    await page.$.flushForTesting();
    await microtasksFinished();

    assertEquals(newUserInfo.title, testElement.$.title.textContent);
    assertEquals(newUserInfo.avatarUrl, testElement.$.avatar.src);
  });

  test('CallsCelebrationFinishedWhenAnimationsDisabled', async function() {
    loadTimeData.overrideValues({disableAnimations: true});
    await createSignInCelebrationAppElement();

    await handler.whenCalled('signInCelebrationFinished');
  });

  test('CallsCelebrationFinishedWhenAnimationCompletes', async function() {
    const testElement = await createSignInCelebrationAppElement();

    testElement.$.avatarAnimation.dispatchEvent(
        new CustomEvent('cr-lottie-completed'));

    await handler.whenCalled('signInCelebrationFinished');
  });
});
