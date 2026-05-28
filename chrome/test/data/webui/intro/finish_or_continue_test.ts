// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://intro/finish_or_continue/app.js';

import type {FinishOrContinueAppElement} from 'chrome://intro/finish_or_continue/app.js';
import {isWindows} from 'chrome://resources/js/platform.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('FinishOrContinueTest', function() {
  let testElement: FinishOrContinueAppElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testElement = document.createElement('finish-or-continue-app');
    document.body.appendChild(testElement);
    return microtasksFinished();
  });

  test('ButtonsExist', function() {
    assertTrue(!!testElement.$.seeMoreTipsButton);
    assertTrue(!!testElement.$.startBrowsingButton);
    assertTrue(
        testElement.$.startBrowsingButton.classList.contains('action-button'));
  });

  test('ButtonsOrder', function() {
    const buttonContainer = testElement.$.buttonContainer;
    assertTrue(!!buttonContainer);

    const buttons = buttonContainer.querySelectorAll('cr-button');
    assertEquals(2, buttons.length);

    if (isWindows) {
      assertEquals(testElement.$.startBrowsingButton, buttons[0]);
      assertEquals(testElement.$.seeMoreTipsButton, buttons[1]);
    } else {
      assertEquals(testElement.$.seeMoreTipsButton, buttons[0]);
      assertEquals(testElement.$.startBrowsingButton, buttons[1]);
    }
  });
});
