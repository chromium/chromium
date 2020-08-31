// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jimmyxgong): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://diagnostics/diagnostics_app.js';

suite('DiagnosticsAppTest', () => {
  /** @type {?DiagnosticsApp} */
  let page = null;

  setup(function() {
    PolymerTest.clearBody();
    page = document.createElement('diagnostics-app');
    document.body.appendChild(page);
  });

  teardown(function() {
    page.remove();
    page = null;
  });

  test('LandingPageLoaded', () => {
    // TODO(jimmyxgong): Remove this stub test once the page has more
    // capabilities to test.
    assertEquals('Diagnostics', page.$$('#header').textContent);
  });
});