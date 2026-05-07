// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://intro/sign_in_celebration/app.js';

import {PageCallbackRouter, PageHandlerRemote} from 'chrome://intro/sign_in_celebration.mojom-webui.js';
import type {PageRemote} from 'chrome://intro/sign_in_celebration.mojom-webui.js';
import {SignInCelebrationBrowserProxyImpl} from 'chrome://intro/sign_in_celebration/sign_in_celebration_browser_proxy.js';
import type {SignInCelebrationAppElement} from 'chrome://intro/sign_in_celebration/app.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('SignInCelebrationTest', function() {
  let testElement: SignInCelebrationAppElement;
  let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  let page: PageRemote;

  const userInfo = {
    avatarUrl: 'http://example.com/image.png',
    title: 'Welcome, Test User',
  };

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    const callbackRouter = new PageCallbackRouter();
    handler = TestMock.fromClass(PageHandlerRemote);
    page = callbackRouter.$.bindNewPipeAndPassRemote();

    SignInCelebrationBrowserProxyImpl.setInstance({
      callbackRouter: callbackRouter,
      handler: handler,
      matchMedia: (query: string) => window.matchMedia(query),
    });

    handler.setPromiseResolveFor(
        'getSignInCelebrationUserInfo', {userInfo: userInfo});

    testElement = document.createElement('sign-in-celebration-app');
    document.body.appendChild(testElement);
    return microtasksFinished();
  });

  test('SetsInitialUserInfo', async function() {
    await handler.whenCalled('getSignInCelebrationUserInfo');
    await microtasksFinished();

    assertEquals(userInfo.title, testElement.$.title.textContent);
    assertEquals(userInfo.avatarUrl, testElement.$.avatar.src);
  });

  test('UpdatesUserInfo', async function() {
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
});
