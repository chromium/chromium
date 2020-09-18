// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jimmyxgong): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://diagnostics/diagnostics_app.js';

import {SystemDataProviderInterface} from 'chrome://diagnostics/diagnostics_types.js';
import {FakeMethodResolver} from 'chrome://diagnostics/fake_method_resolver.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {getSystemDataProvider, setSystemDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';

suite('DiagnosticsFakeMethodResolver', () => {
  /** @type {?FakeMethodResolver} */
  let resolver = null;

  setup(function() {
    resolver = new FakeMethodResolver();
  });

  teardown(function() {
    resolver = null;
  });

  test('AddingMethodNoResult', () => {
    resolver.register('foo');
    return resolver.resolveMethod('foo').then((result) => {
      assertEquals(undefined, result);
    });
  });

  test('AddingMethodWithResult', () => {
    resolver.register('foo');
    const expected = {'foo': 'bar'};
    resolver.setResult('foo', expected);
    return resolver.resolveMethod('foo').then((result) => {
      assertEquals(expected, result);
    });
  });

  test('AddingTwoMethodCallingOne', () => {
    resolver.register('foo');
    resolver.register('bar');
    const expected = {'fooKey': 'fooValue'};
    resolver.setResult('foo', expected);
    return resolver.resolveMethod('foo').then((result) => {
      assertEquals(expected, result);
    });
  });
});

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

suite('FakeSystemDataProviderTest', () => {
  /** @type {?FakeSystemDataProvider} */
  let provider = null;

  setup(function() {
    provider = new FakeSystemDataProvider();
  });

  teardown(function() {
    provider = null;
  });

  test('GetSystemInfo', () => {
    /** @type {!DeviceCapabilities} */
    const capabilities = {
      has_battery: true,
    };

    /** @type {!VersionInfo} */
    const version = {
      milestone_version: 'M97',
    };

    /** @type {!SystemInfo} */
    const expected = {
      board_name: 'BestBoard',
      cpu_model: 'SuperFast CPU',
      total_memory_kib: 9999,
      cores_number: 4,
      version_info: version,
      device_capabilities: capabilities,
    };

    provider.setFakeSystemInfo(expected);
    return provider.getSystemInfo().then((systemInfo) => {
      assertDeepEquals(expected, systemInfo);
    });
  });
});
