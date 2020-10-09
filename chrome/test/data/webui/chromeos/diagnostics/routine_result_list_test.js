// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jimmyxgong): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://diagnostics/routine_result_list.js';
import {flushTasks} from 'chrome://test/test_util.m.js';

suite('RoutineResultListTest', () => {
  /** @type {?HTMLElement} */
  let routineResultListElement = null;

  setup(function() {
    PolymerTest.clearBody();
  });

  teardown(function() {
    if (routineResultListElement) {
      routineResultListElement.remove();
    }
    routineResultListElement = null;
  });

  function initializeRoutineResultList() {
    assertFalse(!!routineResultListElement);

    // Add the entry to the DOM.
    routineResultListElement = document.createElement('routine-result-list');
    assertTrue(!!routineResultListElement);
    document.body.appendChild(routineResultListElement);

    return flushTasks();
  }

  test('ElementRendered', () => {
    return initializeRoutineResultList().then(() => {
      // Verify the element rendered.
      let div = routineResultListElement.$$('#resultListContainer');
      assertTrue(!!div);
    });
  });
});
