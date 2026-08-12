// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://intro/welcome/app.js';

import {browserProxyFactory as welcomeMojoProxyFactory, WelcomePageHandlerRemote} from 'chrome://intro/welcome.mojom-webui.js';
import type {WelcomeAppElement} from 'chrome://intro/welcome/app.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('WelcomeTest', function() {
  let testElement: WelcomeAppElement;
  let handler: TestMock<WelcomePageHandlerRemote>&WelcomePageHandlerRemote;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    handler = TestMock.fromClass(WelcomePageHandlerRemote);
    welcomeMojoProxyFactory.setInstance({handler});

    testElement = document.createElement('welcome-app');
    document.body.appendChild(testElement);
    await microtasksFinished();
  });

  test('ElementsExist', function() {
    const acceptButton = testElement.$.acceptButton;
    assertTrue(acceptButton.classList.contains('action-button'));
    assertFalse(acceptButton.hasAttribute('disabled'));
  });

  test('AcceptButtonClicked', async function() {
    const acceptButton = testElement.$.acceptButton;
    assertFalse(acceptButton.hasAttribute('disabled'));

    acceptButton.click();
    await microtasksFinished();

    assertEquals(1, handler.getCallCount('continue'));
    assertTrue(acceptButton.hasAttribute('disabled'));
  });
});

