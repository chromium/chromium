// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jimmyxgong): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://diagnostics/diagnostics_app.js';

import {SystemDataProviderInterface} from 'chrome://diagnostics/diagnostics_types.js';
import {getSystemDataProvider, setSystemDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';

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

suite('FakeMojoProviderTest', () => {
  test('SettingGettingTestProvider', () => {
    // TODO(zentaro): Replace with fake when built.
    let fake_provider =
        /** @type {SystemDataProviderInterface} */ (new Object());
    setSystemDataProviderForTesting(fake_provider);
    assertEquals(fake_provider, getSystemDataProvider());
  });
});
