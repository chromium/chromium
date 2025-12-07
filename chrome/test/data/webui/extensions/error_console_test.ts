// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://extensions/extensions.js';

import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {findMatches} from './test_util.js';

/** @fileoverview Suite of tests for the extensions error console. */
suite('CrExtensionsErrorConsoleTest', function() {
  const STACK_ERRORS: string = 'li';
  const ACTIVE_ERROR_IN_STACK: string = 'li[tabindex="0"]';

  // Initialize an extension activity log item before each test.
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Use error_console/deep_stack_trace extension for testing.
    window.history.replaceState(
        {}, '', '?errors=oehidglfoeondlkoeloailjdmmghacge');
    const manager = document.createElement('extensions-manager');
    document.body.appendChild(manager);
    // Wait for the first view to be active before starting tests.
    return manager.$.viewManager.querySelector('.active') ?
        Promise.resolve() :
        eventToPromise('view-enter-start', manager);
  });

  test('SingleErrorLayout', () => {
    const initialFocus = findMatches(document, ACTIVE_ERROR_IN_STACK)[0];
    assertTrue(!!initialFocus);
    assertEquals(1, findMatches(document, ACTIVE_ERROR_IN_STACK).length);
    assertEquals(1, findMatches(document, STACK_ERRORS).length);
  });
});
