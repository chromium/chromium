// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BatteryChargeStatus, BatteryHealth, CpuUsage, ExternalPowerSource, MemoryUsage, SystemInfo} from './diagnostics_types.js'

/* @type {!Array<!BatteryChargeStatus>} */
export const fakeBatteryChargeStatus = [
  {
    charge_full_now_milliamp_hours: 5700,
    charge_now_milliamp_hours: 4200,
    current_now_milliamps: 1123,
    power_adapter_status: ExternalPowerSource.kAc,
    power_time: '3h 15m',
  },
  {
    charge_full_now_milliamp_hours: 5700,
    charge_now_milliamp_hours: 4500,
    current_now_milliamps: 1123,
    power_adapter_status: ExternalPowerSource.kAc,
    power_time: '3h 01m',
  },
  {
    charge_full_now_milliamp_hours: 5700,
    charge_now_milliamp_hours: 4800,
    current_now_milliamps: 1123,
    power_adapter_status: ExternalPowerSource.kAc,
    power_time: '2h 45m',
  }
];

/* @type {!Array<!BatteryHealth>} */
export const fakeBatteryHealth = [{
  battery_wear_percentage: 7,
  charge_full_design_milliamp_hours: 6000,
  charge_full_now_milliamp_hours: 5700,
  cycle_count: 73,
}];

/* @type {!BatteryInfo} */
export const fakeBatteryInfo = {
  charge_full_design_milliamp_hours: 6000,
  manufacturer: 'BatterCorp USA',
};

/* @type {!BatteryInfo} */
export const fakeBatteryInfo2 = {
  charge_full_design_milliamp_hours: 9000,
  manufacturer: 'PowerPod 9000',
};

/* @type {!Array<!CpuUsage>} */
export const fakeCpuUsage = [
  {
    cpu_temp_degrees_celcius: 107,
    percent_usage_system: 15,
    percent_usage_user: 20,
  },
  {
    cpu_temp_degrees_celcius: 106,
    percent_usage_system: 30,
    percent_usage_user: 40,
  },
  {
    cpu_temp_degrees_celcius: 107,
    percent_usage_system: 31,
    percent_usage_user: 45,
  },
  {
    cpu_temp_degrees_celcius: 109,
    percent_usage_system: 55,
    percent_usage_user: 24,
  }
];

/* @type {!Array<!MemoryUsage>} */
export const fakeMemoryUsage = [
  {
    available_memory_kib: 57000,
    free_memory_kib: 15000,
    total_memory_kib: 128000,
  },
  {
    available_memory_kib: 52000,
    free_memory_kib: 15000,
    total_memory_kib: 128000,
  },
  {
    available_memory_kib: 53000,
    free_memory_kib: 15000,
    total_memory_kib: 128000,
  },
  {
    available_memory_kib: 65000,
    free_memory_kib: 15000,
    total_memory_kib: 128000,
  }
];

/* @type {!SystemInfo} */
export const fakeSystemInfo = {
  board_name: 'CrOS Board',
  cpu_model_name: 'BestCpu SoFast 1000',
  cpu_threads_count: 8,
  device_capabilities: {has_battery: true},
  total_memory_kib: 128000,
  version: {milestone_version: 'M99'},
};
