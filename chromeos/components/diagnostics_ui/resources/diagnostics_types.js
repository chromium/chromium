// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Type aliases for the mojo API.
 */

/**
 * Type alias for the SystemDataProviderInterface.
 * @typedef {chromeos.diagnostics.mojom.SystemDataProviderInterface}
 */
export let SystemDataProviderInterface;

/**
 * Type alias for DeviceCapabilities.
 * @typedef {chromeos.diagnostics.mojom.DeviceCapabilities}
 */
export let DeviceCapabilities;

/**
 * Type alias for VersionInfo.
 * @typedef {chromeos.diagnostics.mojom.VersionInfo}
 */
export let VersionInfo;

/**
 * Type alias for SystemInfo.
 * @typedef {chromeos.diagnostics.mojom.SystemInfo}
 */
export let SystemInfo;

/**
 * Type alias for ExternalPowerSource.
 * @typedef {chromeos.diagnostics.mojom.ExternalPowerSource}
 */
export let ExternalPowerSource;

/**
 * Type alias for BatteryState.
 * @typedef {chromeos.diagnostics.mojom.BatteryState}
 */
export let BatteryState;

/**
 * Type alias for BatteryInfo.
 * @typedef {chromeos.diagnostics.mojom.BatteryInfo}
 */
export let BatteryInfo;

/**
 * Type alias for BatteryHealthObserver.
 * @typedef {chromeos.diagnostics.mojom.BatteryChargeStatusObserver}
 */
export let BatteryChargeStatusObserver;

/**
 * Type alias for BatteryChargeStatus.
 * @typedef {chromeos.diagnostics.mojom.BatteryChargeStatus}
 */
export let BatteryChargeStatus;

/**
 * Type alias for BatteryHealthObserver.
 * @typedef {chromeos.diagnostics.mojom.BatteryHealthObserver}
 */
export let BatteryHealthObserver;

/**
 * Type alias for BatteryHealth.
 * @typedef {chromeos.diagnostics.mojom.BatteryHealth}
 */
export let BatteryHealth;

/**
 * Type alias for MemoryUsageObserver.
 * @typedef {chromeos.diagnostics.mojom.MemoryUsageObserver}
 */
export let MemoryUsageObserver;

/**
 * Type alias for MemoryUsage.
 * @typedef {chromeos.diagnostics.mojom.MemoryUsage}
 */
export let MemoryUsage;

/**
 * Type alias for CpuUsageObserver.
 * @typedef {chromeos.diagnostics.mojom.CpuUsageObserver}
 */
export let CpuUsageObserver;

/**
 * Type alias for CpuUsage.
 * @typedef {chromeos.diagnostics.mojom.CpuUsage}
 */
export let CpuUsage;

/**
 * Enumeration of routines.
 * @typedef {chromeos.diagnostics.mojom.RoutineType}
 */
export let RoutineType;

/**
 * Type alias for StandardRoutineResult.
 * @typedef {chromeos.diagnostics.mojom.StandardRoutineResult}
 */
export let StandardRoutineResult;

/**
 * Type alias for PowerRoutineResult.
 * @typedef {chromeos.diagnostics.mojom.PowerRoutineResult}
 */
export let PowerRoutineResult;

/**
 * Type alias for RoutineResult.
 * @typedef {chromeos.diagnostics.mojom.RoutineResult}
 */
export let RoutineResult;

/**
 * Type alias for RoutineResultInfo.
 * @typedef {chromeos.diagnostics.mojom.RoutineResultInfo}
 */
export let RoutineResultInfo;

/**
 * Type alias for RoutineRunner.
 * @typedef {chromeos.diagnostics.mojom.RoutineRunnerInterface}
 */
export let RoutineRunner;

/**
 * Type alias for SystemRoutineControllerInterface.
 * @typedef {chromeos.diagnostics.mojom.SystemRoutineControllerInterface}
 */
export let SystemRoutineControllerInterface;

/**
 * TODO(michaelcheco): Add Cellular properties.
 * @typedef {!Object}
 */
export let CellularStateProperties;

/**
 * TODO(michaelcheco): Add Ethernet properties.
 * @typedef {!Object}
 */
export let EthernetStateProperties;

/**
 * @typedef {{
 *   signalStrength: number,
 *   frequency: number,
 *   ssid: string,
 *   bssid: string,
 * }}
 */
export let WiFiStateProperties;

/**
 * @typedef {{
 *   ipAddress: ?string,
 *   nameServers: ?Array<string>,
 *   subnetMask: ?string,
 *   gateway: ?string,
 * }}
 */
export let IPConfigProperties;

/**
 * @typedef {(
 * !CellularStateProperties|!EthernetStateProperties|!WiFiStateProperties)}
 */
export let NetworkProperties;

/**
 * @typedef {{
 *   state: number,
 *   type: number,
 *   networkProperties: !NetworkProperties,
 *   guid: string,
 *   name: string,
 *   macAddress: string,
 *   ipConfigProperties: ?IPConfigProperties,
 * }}
 */
export let Network;

/**
 * @typedef {{
 *   networkGuids: !Array<string>,
 *   activeGuid: ?string,
 * }}
 */
export let NetworkGuidInfo;

/**
 * Type alias for NetworkListObserver.
 * @typedef {{
 *   onNetworkListChanged: !function(!NetworkGuidInfo)
 * }}
 */
export let NetworkListObserver;

/**
 * Type alias for NetworkStateObserver.
 * @typedef {{
 *   onNetworkStateChanged: !function(!Network)
 * }}
 */
export let NetworkStateObserver;

/**
 * Type of NetworkHealthProviderInterface.ObserveNetworkListFunction function.
 * @typedef {!function(!NetworkListObserver): !Promise}
 */
export let ObserveNetworkListFunction;

/**
 * Type of NetworkHealthProviderInterface.ObserveNetworkFunction function.
 * @typedef {!function(!NetworkStateObserver, !string): !Promise}
 */
export let ObserveNetworkFunction;

/**
 * Type alias for the NetworkHealthProviderInterface.
 * TODO(michaelcheco): Replace with a real mojo type when implemented.
 * @typedef {{
 *   observeNetworkList: !ObserveNetworkListFunction,
 *   observeNetwork: !ObserveNetworkFunction,
 * }}
 */
export let NetworkHealthProviderInterface;

/**
 * @enum {number}
 */
export let NetworkState = {
  kUninitialized: 0,
  kDisabled: 1,
  kProhibited: 2,
  kNotConnected: 3,
  kConnecting: 4,
  kPortal: 5,
  kConnected: 6,
  kOnline: 7,
};

/**
 * @enum {number}
 */
export let NetworkType = {
  kCellular: 0,
  kEthernet: 1,
  kWiFi: 2,
};
