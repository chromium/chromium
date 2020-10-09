// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jimmyxgong): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {FakeSystemRoutineController} from 'chrome://diagnostics/fake_system_routine_controller.js';
import {getSystemDataProvider, getSystemRoutineController, setSystemDataProviderForTesting, setSystemRoutineControllerForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';

suite('FakeMojoProviderTest', () => {
  test('SettingGettingTestProvider', () => {
    // TODO(zentaro): Replace with fake when built.
    let fake_provider =
        /** @type {SystemDataProviderInterface} */ (new Object());
    setSystemDataProviderForTesting(fake_provider);
    assertEquals(fake_provider, getSystemDataProvider());
  });

  test('SettingGettingTestController', () => {
    let fake_controller = new FakeSystemRoutineController();
    setSystemRoutineControllerForTesting(fake_controller);
    assertEquals(fake_controller, getSystemRoutineController());
  });
});