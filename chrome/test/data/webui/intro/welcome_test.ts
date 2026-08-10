// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://intro/welcome/app.js';

import type {WelcomeAppElement} from 'chrome://intro/welcome/app.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('WelcomeTest', function() {
  let testElement: WelcomeAppElement;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testElement = document.createElement('welcome-app');
    document.body.appendChild(testElement);
    await microtasksFinished();
  });

  test('ElementsExist', function() {
    const acceptButton = testElement.$.acceptButton;
    assertTrue(acceptButton.classList.contains('action-button'));
    assertFalse(acceptButton.hasAttribute('disabled'));
  });
});

