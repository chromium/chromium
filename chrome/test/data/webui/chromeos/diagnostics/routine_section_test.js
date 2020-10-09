// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jimmyxgong): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://diagnostics/routine_section.js';
import {flushTasks} from 'chrome://test/test_util.m.js';

suite('RoutineSectionTest', () => {
  /** @type {?HTMLElement} */
  let routineSectionElement = null;

  setup(function() {
    PolymerTest.clearBody();
  });

  teardown(function() {
    if (routineSectionElement) {
      routineSectionElement.remove();
    }
    routineSectionElement = null;
  });

  function initializeRoutineSection() {
    assertFalse(!!routineSectionElement);

    // Add the entry to the DOM.
    routineSectionElement = document.createElement('routine-section');
    assertTrue(!!routineSectionElement);
    document.body.appendChild(routineSectionElement);

    return flushTasks();
  }

  test('ElementRenders', () => {
    return initializeRoutineSection().then(() => {
      // Verify the element rendered.
      assertTrue(!!routineSectionElement.$$('#routineSection'));
    });
  });
});
