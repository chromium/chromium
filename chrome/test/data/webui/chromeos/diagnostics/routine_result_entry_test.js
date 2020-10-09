// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jimmyxgong): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://diagnostics/routine_result_entry.js';
import {flushTasks} from 'chrome://test/test_util.m.js';

suite('RoutineResultEntryTest', () => {
  /** @type {?HTMLElement} */
  let routineResultEntryElement = null;

  setup(function() {
    PolymerTest.clearBody();
  });

  teardown(function() {
    if (routineResultEntryElement) {
      routineResultEntryElement.remove();
    }
    routineResultEntryElement = null;
  });

  function initializeRoutineResultEntry() {
    assertFalse(!!routineResultEntryElement);

    // Add the entry to the DOM.
    routineResultEntryElement = document.createElement('routine-result-entry');
    assertTrue(!!routineResultEntryElement);
    document.body.appendChild(routineResultEntryElement);

    return flushTasks();
  }

  test('ElementRendered', () => {
    return initializeRoutineResultEntry().then(() => {
      // Verify the element rendered.
      let div = routineResultEntryElement.$$('.entryRow');
      assertTrue(!!div);
    });
  });
});
