// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Types for objects sent from C++ to chrome://policy/logs.
 */

/**
 * The type of the policy log result object. The definition is based on
 * components/policy/core/common/policy_logger.cc Log::GetAsValue()
 */
export interface Log {
  logSource: string;
  logSeverity: string;
  message: string;
  location: string;
  timestamp: string;
}

/**
 * The type of the version info result object. The definition is based on
 * chrome/browser/ui/webui/policy/policy_ui_handler.cc: GetVersionInfo()
 */
export interface VersionInfo {
  version: string;
  revision: string;
  deviceOs: string;
  variations: string[];
}