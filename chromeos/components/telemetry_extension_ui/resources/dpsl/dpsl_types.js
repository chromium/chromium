// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * dpsl.* types definitions.
 */

////////////////////// dpsl.telemetry.* type definitions ///////////////////////

/**
 * Response message containing Backlight Info
 * @typedef {!Array<{
 *   path: string,
 *   maxBrightness: number,
 *   brightness: number,
 * }>}
 */
dpsl.BacklightInfo;

/**
 * Response message containing Battery Info
 * @typedef {{
 *   cycleCount: bigint,
 *   voltageNow: number,
 *   vendor: string,
 *   serialNumber: string,
 *   chargeFullDesign: number,
 *   chargeFull: number,
 *   voltageMinDesign: number,
 *   modelName: string,
 *   chargeNow: number,
 *   currentNow: number,
 *   technology: string,
 *   status: string,
 *   manufactureDate: string,
 *   temperature: bigint
 * }}
 */
dpsl.BatteryInfo;

/**
 * Response message containing Bluetooth Info
 * @typedef {!Array<{
 *   name: string,
 *   address: string,
 *   powered: boolean,
 *   numConnectedDevices: number
 * }>}
 */
dpsl.BluetoothInfo;

/**
 * Response message containing VPD Info
 * @typedef {{
 *   skuNumber: string,
 *   serialNumber: string,
 *   modelName: string
 * }}
 */
dpsl.VpdInfo;

/**
 * Response message containing CPU Info
 * @typedef {{
 *   numTotalThreads: number,
 *   architecture: string,
 *   physicalCpus: Array<Object>
 * }}
 */
dpsl.CpuInfo;

/**
 * Response message containing Fan Info
 * @typedef {!Array<{
 *   speedRpm: number
 * }>}
 */
dpsl.FanInfo;

/**
 * Response message containing Memory Info
 * @typedef {{
 *   totalMemoryKib: number,
 *   freeMemoryKib: number,
 *   availableMemoryKib: number,
 *   pageFaultsSinceLastBoot: bigint
 * }}
 */
dpsl.MemoryInfo;

/**
 * Response message containing BlockDevice Info
 * @typedef {!Array<{
 *   path: string,
 *   size: bigint,
 *   type: string,
 *   manufacturerId: number,
 *   name: string,
 *   serial: string,
 *   bytesReadSinceLastBoot: bigint,
 *   bytesWrittenSinceLastBoot: bigint,
 *   readTimeSecondsSinceLastBoot: bigint,
 *   writeTimeSecondsSinceLastBoot: bigint,
 *   ioTimeSecondsSinceLastBoot: bigint,
 *   discardTimeSecondsSinceLastBoot: bigint
 * }>}
 */
dpsl.BlockDeviceInfo;

/**
 * Response message containing StatefulPartition Info
 * @typedef {{
 *   availableSpace: bigint,
 *   totalSpace: bigint
 * }}
 */
dpsl.StatefulPartitionInfo;

/**
 * Response message containing Timezone Info
 * @typedef {{
 *   posix: string,
 *   region: string
 * }}
 */
dpsl.TimezoneInfo;

/**
 * Union of the Telemetry Info types.
 * @typedef {(!dpsl.BacklightInfo|!dpsl.BatteryInfo|!dpsl.BluetoothInfo|
 *   !dpsl.VpdInfo|!dpsl.CpuInfo|!dpsl.FanInfo|!dpsl.MemoryInfo|
 *   !dpsl.BlockDeviceInfo|!dpsl.StatefulPartitionInfo|!dpsl.TimezoneInfo
 * )}
 */
dpsl.TelemetryInfoTypes;

///////////////////// dpsl.diagnostics.* type definitions /////////////////////

/**
 * Static list of available diagnostics routines (tests).
 * @typedef {!Array<!string>}
 */
dpsl.AvailableRoutinesList;

//////////////////// dpsl.system_events.* type definitions /////////////////////

/**
 * The list of supported system events: ['ac-inserted', 'ac-removed',
 * 'bluetooth-adapter-added', 'bluetooth-adapter-property-changed',
 * 'bluetooth-adapter-removed', 'bluetooth-device-added',
 * 'bluetooth-device-property-changed', 'bluetooth-device-removed',
 * 'lid-closed', 'lid-opened', 'os-resume', 'os-suspend']
]
 * @typedef {!Array<!string>}
 */
dpsl.EventTypes;
