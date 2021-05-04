// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import './system_data_provider.mojom-lite.js';
import './system_routine_controller.mojom-lite.js';

import {assert} from 'chrome://resources/js/assert.m.js';

import {NetworkHealthProviderInterface, PowerRoutineResult, RoutineType, StandardRoutineResult, SystemDataProviderInterface, SystemInfo, SystemRoutineControllerInterface} from './diagnostics_types.js';
import {fakeAllNetworksAvailable, fakeBatteryChargeStatus, fakeBatteryHealth, fakeBatteryInfo, fakeCellularNetwork, fakeCpuUsage, fakeEthernetNetwork, fakeMemoryUsage, fakePowerRoutineResults, fakeRoutineResults, fakeSystemInfo, fakeWifiNetwork} from './fake_data.js';
import {FakeNetworkHealthProvider} from './fake_network_health_provider.js';
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
 * @type {?NetworkHealthProviderInterface}
 */
let networkHealthProvider = null;

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

/**
 * @param {!NetworkHealthProviderInterface} testProvider
 */
export function setNetworkHealthProviderForTesting(testProvider) {
  networkHealthProvider = testProvider;
}

function setupFakeNetworkHealthProvider_() {
  const provider = new FakeNetworkHealthProvider();
  // The fake provides a stable state with all networks connected.
  provider.setFakeNetworkGuidInfo([fakeAllNetworksAvailable]);
  provider.setFakeNetworkState('ethernetGuid', [fakeEthernetNetwork]);
  provider.setFakeNetworkState('wifiGuid', [fakeWifiNetwork]);
  provider.setFakeNetworkState('cellularGuid', [fakeCellularNetwork]);

  setNetworkHealthProviderForTesting(provider);
}

/**
 * @return {!NetworkHealthProviderInterface}
 */
export function getNetworkHealthProvider() {
  if (!networkHealthProvider) {
    // TODO(michaelcheco): Instantiate a real mojo interface here.
    setupFakeNetworkHealthProvider_();
  }

  assert(!!networkHealthProvider);
  return networkHealthProvider;
}
