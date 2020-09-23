// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CpuUsage, SystemInfo} from './diagnostics_types.js'

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
