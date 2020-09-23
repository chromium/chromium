// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BatteryChargeStatus, BatteryHealth, CpuUsage, ExternalPowerSource, SystemInfo} from './diagnostics_types.js'

/* @type {!Array<!BatteryChargeStatus>} */
export const fakeBatteryChargeStatus = [{
  charge_full_now_milliamp_hours: 6000,
  charge_now_milliamp_hours: 4200,
  current_now_milliamps: 1123,
  power_adapter_status: ExternalPowerSource.kAc,
  power_time: '3h 15m',
}];

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

/* @type {!Array<!CpuUsage>} */
export const fakeCpuUsage = [{
  cpu_temp_degrees_celcius: 107,
  percent_usage_system: 15,
  percent_usage_user: 20,
}];

/* @type {!SystemInfo} */
export const fakeSystemInfo = {
  board_name: 'CrOS Board',
  cpu_model_name: 'BestCpu SoFast 1000',
  cpu_threads_count: 8,
  device_capabilities: {has_battery: true},
  total_memory_kib: 128000,
  version: {milestone_version: 'M99'},
};
