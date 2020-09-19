// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SystemInfo} from './diagnostics_types.js'

/* @type {!SystemInfo} */
export const fakeSystemInfo = {
  board_name: 'CrOS Board',
  cpu_model_name: 'BestCpu SoFast 1000',
  cpu_threads_count: 8,
  device_capabilities: {has_battery: true},
  total_memory_kib: 128000,
  version: {milestone_version: 'M99'},
};
