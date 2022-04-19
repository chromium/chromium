// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SystemDataProviderInterface} from 'chrome://diagnostics/diagnostics_types.js';
import {FakeNetworkHealthProvider} from 'chrome://diagnostics/fake_network_health_provider.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {FakeSystemRoutineController} from 'chrome://diagnostics/fake_system_routine_controller.js';
import {getNetworkHealthProvider, getSystemDataProvider, getSystemRoutineController, setNetworkHealthProviderForTesting, setSystemDataProviderForTesting, setSystemRoutineControllerForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';

import {assertEquals} from '../../chai_assert.js';

export function fakeMojoProviderTestSuite() {
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
}
