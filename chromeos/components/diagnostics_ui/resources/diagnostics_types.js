// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Type aliases for the mojo API.
 */

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import './input_data_provider.mojom-lite.js';
import './network_health_provider.mojom-lite.js'
import './system_data_provider.mojom-lite.js';
import './system_routine_controller.mojom-lite.js';

/**
 * Type alias for the SystemDataProvider.
 * @typedef {chromeos.diagnostics.mojom.SystemDataProvider}
 */
export let SystemDataProvider = chromeos.diagnostics.mojom.SystemDataProvider;

/**
 * Type alias for the SystemDataProviderInterface.
 * @typedef {chromeos.diagnostics.mojom.SystemDataProviderInterface}
 */
export let SystemDataProviderInterface =
    chromeos.diagnostics.mojom.SystemDataProviderInterface;

/**
 * Type alias for DeviceCapabilities.
 * @typedef {chromeos.diagnostics.mojom.DeviceCapabilities}
 */
export let DeviceCapabilities = chromeos.diagnostics.mojom.DeviceCapabilities;

/**
 * Type alias for VersionInfo.
 * @typedef {chromeos.diagnostics.mojom.VersionInfo}
 */
export let VersionInfo = chromeos.diagnostics.mojom.VersionInfo;

/**
 * Type alias for SystemInfo.
 * @typedef {chromeos.diagnostics.mojom.SystemInfo}
 */
export let SystemInfo = chromeos.diagnostics.mojom.SystemInfo;

/**
 * Type alias for ExternalPowerSource.
 * @typedef {chromeos.diagnostics.mojom.ExternalPowerSource}
 */
export let ExternalPowerSource = chromeos.diagnostics.mojom.ExternalPowerSource;

/**
 * Type alias for BatteryState.
 * @typedef {chromeos.diagnostics.mojom.BatteryState}
 */
export let BatteryState = chromeos.diagnostics.mojom.BatteryState;

/**
 * Type alias for BatteryInfo.
 * @typedef {chromeos.diagnostics.mojom.BatteryInfo}
 */
export let BatteryInfo = chromeos.diagnostics.mojom.BatteryInfo;

/**
 * Type alias for BatteryChargeStatusObserver.
 * @typedef {chromeos.diagnostics.mojom.BatteryChargeStatusObserver}
 */
export let BatteryChargeStatusObserver =
    chromeos.diagnostics.mojom.BatteryChargeStatusObserver;

/**
 * Type alias for BatteryChargeStatusObserverRemote.
 * @typedef {chromeos.diagnostics.mojom.BatteryChargeStatusObserverRemote}
 */
export let BatteryChargeStatusObserverRemote =
    chromeos.diagnostics.mojom.BatteryChargeStatusObserverRemote;

/**
 * Type alias for BatteryChargeStatusObserverInterface.
 * @typedef {chromeos.diagnostics.mojom.BatteryChargeStatusObserverInterface}
 */
export let BatteryChargeStatusObserverInterface =
    chromeos.diagnostics.mojom.BatteryChargeStatusObserverInterface;

/**
 * Type alias for BatteryChargeStatusObserverReceiver.
 * @typedef {chromeos.diagnostics.mojom.BatteryChargeStatusObserverReceiver}
 */
export let BatteryChargeStatusObserverReceiver =
    chromeos.diagnostics.mojom.BatteryChargeStatusObserverReceiver;

/**
 * Type alias for BatteryChargeStatus.
 * @typedef {chromeos.diagnostics.mojom.BatteryChargeStatus}
 */
export let BatteryChargeStatus = chromeos.diagnostics.mojom.BatteryChargeStatus;

/**
 * Type alias for BatteryHealthObserver.
 * @typedef {chromeos.diagnostics.mojom.BatteryHealthObserver}
 */
export let BatteryHealthObserver =
    chromeos.diagnostics.mojom.BatteryHealthObserver;

/**
 * Type alias for BatteryHealthObserver.
 * @typedef {chromeos.diagnostics.mojom.BatteryHealthObserverRemote}
 */
export let BatteryHealthObserverRemote =
    chromeos.diagnostics.mojom.BatteryHealthObserverRemote;

/**
 * Type alias for BatteryHealthObserverInterface.
 * @typedef {chromeos.diagnostics.mojom.BatteryHealthObserverInterface}
 */
export let BatteryHealthObserverInterface =
    chromeos.diagnostics.mojom.BatteryHealthObserverInterface;

/**
 * Type alias for BatteryHealthObserverReceiver.
 * @typedef {chromeos.diagnostics.mojom.BatteryHealthObserverReceiver}
 */
export let BatteryHealthObserverReceiver =
    chromeos.diagnostics.mojom.BatteryHealthObserverReceiver;

/**
 * Type alias for BatteryHealth.
 * @typedef {chromeos.diagnostics.mojom.BatteryHealth}
 */
export let BatteryHealth = chromeos.diagnostics.mojom.BatteryHealth;

/**
 * Type alias for MemoryUsageObserver.
 * @typedef {chromeos.diagnostics.mojom.MemoryUsageObserver}
 */
export let MemoryUsageObserver = chromeos.diagnostics.mojom.MemoryUsageObserver;

/**
 * Type alias for MemoryUsageObserverRemote.
 * @typedef {chromeos.diagnostics.mojom.MemoryUsageObserverRemote}
 */
export let MemoryUsageObserverRemote =
    chromeos.diagnostics.mojom.MemoryUsageObserverRemote;

/**
 * Type alias for MemoryUsageObserverInterface.
 * @typedef {chromeos.diagnostics.mojom.MemoryUsageObserverInterface}
 */
export let MemoryUsageObserverInterface =
    chromeos.diagnostics.mojom.MemoryUsageObserverInterface;

/**
 * Type alias for MemoryUsageObserverReceiver.
 * @typedef {chromeos.diagnostics.mojom.MemoryUsageObserverReceiver}
 */
export let MemoryUsageObserverReceiver =
    chromeos.diagnostics.mojom.MemoryUsageObserverReceiver;

/**
 * Type alias for MemoryUsage.
 * @typedef {chromeos.diagnostics.mojom.MemoryUsage}
 */
export let MemoryUsage = chromeos.diagnostics.mojom.MemoryUsage;

/**
 * Type alias for CpuUsageObserver.
 * @typedef {chromeos.diagnostics.mojom.CpuUsageObserver}
 */
export let CpuUsageObserver = chromeos.diagnostics.mojom.CpuUsageObserver;

/**
 * Type alias for CpuUsageObserverRemote.
 * @typedef {chromeos.diagnostics.mojom.CpuUsageObserverRemote}
 */
export let CpuUsageObserverRemote =
    chromeos.diagnostics.mojom.CpuUsageObserverRemote;

/**
 * Type alias for CpuUsageObserverInterface.
 * @typedef {chromeos.diagnostics.mojom.CpuUsageObserverInterface}
 */
export let CpuUsageObserverInterface =
    chromeos.diagnostics.mojom.CpuUsageObserverInterface;

/**
 * Type alias for CpuUsageObserverReceiver.
 * @typedef {chromeos.diagnostics.mojom.CpuUsageObserverReceiver}
 */
export let CpuUsageObserverReceiver =
    chromeos.diagnostics.mojom.CpuUsageObserverReceiver;

/**
 * Type alias for CpuUsage.
 * @typedef {chromeos.diagnostics.mojom.CpuUsage}
 */
export let CpuUsage = chromeos.diagnostics.mojom.CpuUsage;

/**
 * Enumeration of routines.
 * @typedef {chromeos.diagnostics.mojom.RoutineType}
 */
export let RoutineType = chromeos.diagnostics.mojom.RoutineType;

/**
 * Type alias for StandardRoutineResult.
 * @typedef {chromeos.diagnostics.mojom.StandardRoutineResult}
 */
export let StandardRoutineResult =
    chromeos.diagnostics.mojom.StandardRoutineResult;

/**
 * Type alias for PowerRoutineResult.
 * @typedef {chromeos.diagnostics.mojom.PowerRoutineResult}
 */
export let PowerRoutineResult = chromeos.diagnostics.mojom.PowerRoutineResult;

/**
 * Type alias for RoutineResult.
 * @typedef {chromeos.diagnostics.mojom.RoutineResult}
 */
export let RoutineResult = chromeos.diagnostics.mojom.RoutineResult;

/**
 * Type alias for RoutineResultInfo.
 * @typedef {chromeos.diagnostics.mojom.RoutineResultInfo}
 */
export let RoutineResultInfo = chromeos.diagnostics.mojom.RoutineResultInfo;

/**
 * Type alias for RoutineRunnerInterface.
 * @typedef {chromeos.diagnostics.mojom.RoutineRunnerInterface}
 */
export let RoutineRunnerInterface =
    chromeos.diagnostics.mojom.RoutineRunnerInterface;

/**
 * Type alias for RoutineRunnerRemote.
 * @typedef {chromeos.diagnostics.mojom.RoutineRunnerRemote}
 */
export let RoutineRunnerRemote = chromeos.diagnostics.mojom.RoutineRunnerRemote;

/**
 * Type alias for RoutineRunnerReceiver.
 * @typedef {chromeos.diagnostics.mojom.RoutineRunnerReceiver}
 */
export let RoutineRunnerReceiver =
    chromeos.diagnostics.mojom.RoutineRunnerReceiver;

/**
 * Type alias for SystemRoutineController.
 * @typedef {chromeos.diagnostics.mojom.SystemRoutineController}
 */
export let SystemRoutineController =
    chromeos.diagnostics.mojom.SystemRoutineController;

/**
 * Type alias for SystemRoutineControllerInterface.
 * @typedef {chromeos.diagnostics.mojom.SystemRoutineControllerInterface}
 */
export let SystemRoutineControllerInterface =
    chromeos.diagnostics.mojom.SystemRoutineControllerInterface;

/**
 * Type alias for NetworkListObserver.
 * @typedef {chromeos.diagnostics.mojom.NetworkListObserverRemote}
 */
export let NetworkListObserverRemote =
    chromeos.diagnostics.mojom.NetworkListObserverRemote;

/**
 * Type alias for NetworkStateObserver.
 * @typedef {chromeos.diagnostics.mojom.NetworkStateObserverRemote}
 */
export let NetworkStateObserverRemote =
    chromeos.diagnostics.mojom.NetworkStateObserverRemote;

/**
 * Type alias for Network.
 * @typedef {chromeos.diagnostics.mojom.Network}
 */
export let Network = chromeos.diagnostics.mojom.Network;

/**
 * Type alias for NetworkHealthProvider.
 * @typedef {chromeos.diagnostics.mojom.NetworkHealthProvider}
 */
export let NetworkHealthProvider =
    chromeos.diagnostics.mojom.NetworkHealthProvider;

/**
 * Type alias for NetworkHealthProviderInterface.
 * @typedef {chromeos.diagnostics.mojom.NetworkHealthProviderInterface}
 */
export let NetworkHealthProviderInterface =
    chromeos.diagnostics.mojom.NetworkHealthProviderInterface;

/**
 * Type alias for NetworkState.
 * @typedef {chromeos.diagnostics.mojom.NetworkState}
 */
export let NetworkState = chromeos.diagnostics.mojom.NetworkState;

/**
 * Type alias for NetworkType
 * @typedef {chromeos.diagnostics.mojom.NetworkType}
 */
export let NetworkType = chromeos.diagnostics.mojom.NetworkType;

/**
 * Type alias for NetworkListObserverReceiver.
 * @typedef {chromeos.diagnostics.mojom.NetworkListObserverReceiver}
 */
export let NetworkListObserverReceiver =
    chromeos.diagnostics.mojom.NetworkListObserverReceiver;

/**
 * Type alias for NetworkListObserverInterface.
 * @typedef {chromeos.diagnostics.mojom.NetworkListObserverInterface}
 */
export let NetworkListObserverInterface =
    chromeos.diagnostics.mojom.NetworkListObserverInterface;

/**
 * Type alias for NetworkStateObserverInterface.
 * @typedef {chromeos.diagnostics.mojom.NetworkStateObserverInterface}
 */
export let NetworkStateObserverInterface =
    chromeos.diagnostics.mojom.NetworkStateObserverInterface;

/**
 * Type alias for NetworkStateObserverReceiver.
 * @typedef {chromeos.diagnostics.mojom.NetworkStateObserverReceiver}
 */
export let NetworkStateObserverReceiver =
    chromeos.diagnostics.mojom.NetworkStateObserverReceiver;

/**
 * @typedef {{
 *   networkGuids: !Array<string>,
 *   activeGuid: string,
 * }}
 */
export let NetworkGuidInfo;

/**
 * Type alias for ConnectionType.
 * @typedef {chromeos.diagnostics.mojom.ConnectionType}
 */
export let ConnectionType =
    chromeos.diagnostics.mojom.ConnectionType;

/**
 * Type alias for PhysicalLayout.
 * @typedef {chromeos.diagnostics.mojom.PhysicalLayout}
 */
export let PhysicalLayout = chromeos.diagnostics.mojom.PhysicalLayout;

/**
 * Type alias for KeyboardInfo.
 * @typedef {chromeos.diagnostics.mojom.KeyboardInfo}
 */
export let KeyboardInfo =
    chromeos.diagnostics.mojom.KeyboardInfo;

/**
 * Type alias for TouchDeviceType.
 * @typedef {chromeos.diagnostics.mojom.TouchDeviceType}
 */
export let TouchDeviceType =
    chromeos.diagnostics.mojom.TouchDeviceType;

/**
 * Type alias for TouchDeviceInfo.
 * @typedef {chromeos.diagnostics.mojom.TouchDeviceInfo}
 */
export let TouchDeviceInfo =
    chromeos.diagnostics.mojom.TouchDeviceInfo;

/**
 * Type alias for ConnectedDevicesObserver.
 * @typedef {chromeos.diagnostics.mojom.ConnectedDevicesObserver}
 */
export let ConnectedDevicesObserver = chromeos.diagnostics.mojom.CpuUsageObserver;

/**
 * Type alias for ConnectedDevicesObserverRemote.
 * @typedef {chromeos.diagnostics.mojom.ConnectedDevicesObserverRemote}
 */
export let ConnectedDevicesObserverRemote =
    chromeos.diagnostics.mojom.ConnectedDevicesObserverRemote;

/**
 * Type alias for ConnectedDevicesObserverInterface.
 * @typedef {chromeos.diagnostics.mojom.ConnectedDevicesObserverInterface}
 */
export let ConnectedDevicesObserverInterface =
    chromeos.diagnostics.mojom.ConnectedDevicesObserverInterface;

/**
 * Type alias for ConnectedDevicesObserverReceiver.
 * @typedef {chromeos.diagnostics.mojom.ConnectedDevicesObserverReceiver}
 */
export let ConnectedDevicesObserverReceiver =
    chromeos.diagnostics.mojom.ConnectedDevicesObserverReceiver;


/**
 * Type alias for the the response from InputDataProvider.GetConnectedDevices.
 * @typedef {{keyboards: !Array<!KeyboardInfo>,
 *            touchDevices: !Array<!TouchDeviceInfo>}}
 */
export let GetConnectedDevicesResponse;

/**
 * Type alias for InputDataProviderInterface.
 * @typedef {chromeos.diagnostics.mojom.InputDataProviderInterface}
 */
export let InputDataProviderInterface =
    chromeos.diagnostics.mojom.InputDataProviderInterface;
