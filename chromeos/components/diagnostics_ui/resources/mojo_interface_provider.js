// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import './system_data_provider.mojom-lite.js';
import './system_routine_controller.mojom-lite.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {SystemDataProviderInterface, SystemInfo, SystemRoutineControllerInterface} from './diagnostics_types.js';
import {fakeBatteryChargeStatus, fakeBatteryHealth, fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage, fakeRoutineResults, fakeSystemInfo} from './fake_data.js';
import {FakeSystemDataProvider} from './fake_system_data_provider.js';
import {FakeSystemRoutineController} from './fake_system_routine_controller.js';

/**
 * @fileoverview
 * Provides singleton access to mojo interfaces with the ability
 * to override them with test/fake implementations.
 */

/**
 * Sets up a FakeSystemDataProvider to be used at runtime.
 * TODO(zentaro): Remove once mojo bindings are implemented.
 */
function setupFakeSystemDataProvider_() {
  // Create provider.
  let provider = new FakeSystemDataProvider();

  // Setup fake method data.
  provider.setFakeBatteryChargeStatus(fakeBatteryChargeStatus);
  provider.setFakeBatteryHealth(fakeBatteryHealth);
  provider.setFakeBatteryInfo(fakeBatteryInfo);
  provider.setFakeCpuUsage(fakeCpuUsage);
  provider.setFakeMemoryUsage(fakeMemoryUsage);
  provider.setFakeSystemInfo(fakeSystemInfo);

  // Start the timers to generate some observations.
  provider.startTriggerIntervals();

  // Set the fake provider.
  setSystemDataProviderForTesting(provider);
}

/**
 * Sets up a FakeSystemRoutineController to be used at runtime.
 * TODO(zentaro): Remove once mojo bindings are implemented.
 */
function setupFakeSystemRoutineController_() {
  // Create controller.
  let controller = new FakeSystemRoutineController();

  // Add a small delay while running fake routines.
  controller.setDelayTimeInMillisecondsForTesting(2000);

  // Add fake results for routines.
  for (const [routine, result] of fakeRoutineResults.entries()) {
    controller.setFakeStandardRoutineResult(routine, result);
  }

  // Set the fake controller.
  setSystemRoutineControllerForTesting(controller);
}

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
    // TODO(zentaro): Instantiate a real mojo interface here.
    setupFakeSystemRoutineController_();
  }

  assert(!!systemRoutineController);
  return systemRoutineController;
}
