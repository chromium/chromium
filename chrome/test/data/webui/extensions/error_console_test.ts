// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://extensions/extensions.js';

import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {findMatches} from './test_util.js';

/** @fileoverview Suite of tests for the extensions error console. */
suite('CrExtensionsErrorConsoleTest', function() {
  const STACK_ERRORS: string = 'li';
  const ACTIVE_ERROR_IN_STACK: string = 'li[tabindex="0"]';

  // Initialize an extension activity log item before each test.
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    window.history.replaceState(
        {}, '', '?errors=oehidglfoeondlkoeloailjdmmghacge');
    const manager = document.createElement('extensions-manager');
    document.body.appendChild(manager);
    // Wait for the first view to be active before starting tests.
    return manager.$.viewManager.querySelector('.active') ?
        Promise.resolve() :
        eventToPromise('view-enter-start', manager);
  });

  test('TestUpDownErrors', async () => {
    const initialFocus = findMatches(document, ACTIVE_ERROR_IN_STACK)[0];
    assertTrue(!!initialFocus);
    assertEquals(1, findMatches(document, ACTIVE_ERROR_IN_STACK).length);
    assertEquals(4, findMatches(document, STACK_ERRORS).length);

    // Pressing up when the first item is focused should NOT change focus.
    keyDownOn(initialFocus, 38, [], 'ArrowUp');
    await microtasksFinished();
    assertEquals(initialFocus, findMatches(document, ACTIVE_ERROR_IN_STACK)[0]);

    // Pressing down when the first item is focused should change focus.
    keyDownOn(initialFocus, 40, [], 'ArrowDown');
    await microtasksFinished();
    assertNotEquals(
        initialFocus, findMatches(document, ACTIVE_ERROR_IN_STACK)[0]);

    // Pressing up when the second item is focused should focus the first again.
    keyDownOn(initialFocus, 38, [], 'ArrowUp');
    await microtasksFinished();
    assertEquals(initialFocus, findMatches(document, ACTIVE_ERROR_IN_STACK)[0]);
  });
});
