// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {FakeNetworkHealthProvider} from 'chrome://diagnostics/fake_network_health_provider.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {FakeSystemRoutineController} from 'chrome://diagnostics/fake_system_routine_controller.js';
import {getNetworkHealthProvider, getSystemDataProvider, getSystemRoutineController, setNetworkHealthProviderForTesting, setSystemDataProviderForTesting, setSystemRoutineControllerForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('fakeMojoProviderTestSuite', function() {
  test('SettingGettingTestProvider', () => {
    const fake_provider = new FakeSystemDataProvider();
    setSystemDataProviderForTesting(fake_provider);
    assertEquals(fake_provider, getSystemDataProvider());
  });

  test('SettingGettingTestController', () => {
    const fake_controller = new FakeSystemRoutineController();
    setSystemRoutineControllerForTesting(fake_controller);
    assertEquals(fake_controller, getSystemRoutineController());
  });

  test('SettingGettingTestNetworkHealthProvider', () => {
    const fake_provider = new FakeNetworkHealthProvider();
    setNetworkHealthProviderForTesting(fake_provider);
    assertEquals(fake_provider, getNetworkHealthProvider());
  });
});
