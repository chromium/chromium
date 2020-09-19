// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Type aliases for the mojo API.
 *
 * TODO(zentaro): When the fake API is replaced by mojo these can be
 * re-aliased to the corresponding mojo types, or replaced by them.
 */

/**
 * Type alias for the SystemDataProviderInterface.
 * TODO(zentaro): Replace with a real mojo type when implemented.
 * @typedef {{
 *   getSystemInfo: !function(): !Promise<!SystemInfo>
 * }}
 */
export let SystemDataProviderInterface;

/**
 * Type alias for DeviceCapabilities.
 * @typedef {{
 *   has_battery: boolean,
 * }}
 */
export let DeviceCapabilities;

/**
 * Type alias for VersionInfo.
 * @typedef {{
 *   milestone_version: string,
 * }}
 */
export let VersionInfo;

/**
 * Type alias for SystemInfo.
 * @typedef {{
 *   board_name: string,
 *   cpu_model_name: string,
 *   cpu_threads_count: number,
 *   device_capabilities: DeviceCapabilities,
 *   total_memory_kib: number,
 *   version: VersionInfo,
 * }}
 */
export let SystemInfo;
