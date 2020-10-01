// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jimmyxgong): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://diagnostics/diagnostics_app.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks} from 'chrome://test/test_util.m.js';

suite('DiagnosticsAppTest', () => {
  /** @type {?DiagnosticsApp} */
  let page = null;

  setup(() => {
    PolymerTest.clearBody();
    page = document.createElement('diagnostics-app');
    assertTrue(!!page);
    document.body.appendChild(page);
  });

  teardown(() => {
    page.remove();
    page = null;
  });

  test('LandingPageLoaded', () => {
    // Verify the overview card is in the page.
    const overview = page.$$('#overviewCard');
    assertTrue(!!overview);

    // Verify the memory card is in the page.
    const memory = page.$$('#memoryCard');
    assertTrue(!!memory);

    // Verify the CPU card is in the page.
    const cpu = page.$$('#cpuCard');
    assertTrue(!!cpu);

    // Verify the battery status card is in the page.
    const batteryStatus = page.$$('#batteryStatusCard');
    assertTrue(!!batteryStatus);

    // Verify the session log button is in the page.
    const sessionLog = page.$$('.session-log-button');
    assertTrue(!!sessionLog);
  });
});
