// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import './system_data_provider.mojom-lite.js';
import './system_routine_controller.mojom-lite.js';

import {assert} from 'chrome://resources/js/assert.m.js';

import {PowerRoutineResult, RoutineType, StandardRoutineResult, SystemDataProviderInterface, SystemInfo, SystemRoutineControllerInterface} from './diagnostics_types.js';
import {fakeBatteryChargeStatus, fakeBatteryHealth, fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage, fakePowerRoutineResults, fakeRoutineResults, fakeSystemInfo} from './fake_data.js';
import {FakeSystemDataProvider} from './fake_system_data_provider.js';
import {FakeSystemRoutineController} from './fake_system_routine_controller.js';

/**
 * @fileoverview
 * Provides singleton access to mojo interfaces with the ability
 * to override them with test/fake implementations.
 */

/**
 * @type {?SystemDataProviderInterface}
 */
let systemDataProvider = null;

/**
 * @type {?SystemRoutineControllerInterface}
 */
let systemRoutineController = null;

/**
 * @param {!SystemDataProviderInterface} testProvider
 */
export function setSystemDataProviderForTesting(testProvider) {
  systemDataProvider = testProvider;
}

/**
 * @return {!SystemDataProviderInterface}
 */
export function getSystemDataProvider() {
  if (!systemDataProvider) {
    systemDataProvider =
        chromeos.diagnostics.mojom.SystemDataProvider.getRemote();
  }

  assert(!!systemDataProvider);
  return systemDataProvider;
}

/**
 * @param {!SystemRoutineControllerInterface} testController
 */
export function setSystemRoutineControllerForTesting(testController) {
  systemRoutineController = testController;
}

/**
 * @return {!SystemRoutineControllerInterface}
 */
export function getSystemRoutineController() {
  if (!systemRoutineController) {
    systemRoutineController =
        chromeos.diagnostics.mojom.SystemRoutineController.getRemote();
  }

  assert(!!systemRoutineController);
  return systemRoutineController;
}
