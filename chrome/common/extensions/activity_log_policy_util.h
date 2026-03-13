// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_ACTIVITY_LOG_POLICY_UTIL_H_
#define CHROME_COMMON_EXTENSIONS_ACTIVITY_LOG_POLICY_UTIL_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "extensions/common/dom_action_types.h"

namespace extensions::activity_log_policy_util {

// A shared utility namespace for identifying and normalizing high-risk
// extension activities for enterprise telemetry.
//
// This logic is shared between:
// 1. The Renderer (ChromePolicyActivityLogFilterDelegate): To decide which
//    activities are important enough to send over IPC to the browser.
// 2. The Browser (ActivityLogIngester): To verify renderer IPCs and
//    transform them into structured telemetry signals.

// Defines the categories of high-risk activity signals.
enum class TelemetrySignalType {
  kNone,             // Not a high-risk event.
  kDOMAccess,        // Matches confidentiality-sensitive activities.
  kScriptInjection,  // Matches integrity-sensitive activities.
};

// Returns the specific telemetry signal type for a given extension
// activity.
// The `api_name` parameter contains the name of the API or action
// being invoked (e.g., "scripting.executeScript" or
// "blinkSetAttribute").
// The `args_unsafe` parameter contains the raw argument list and
// should be processed carefully to extract meaningful context
// (e.g., tag and attribute names for DOM mutations).
// The `action_type` parameter is used to distinguish between sensitive
// READs and less interesting WRITEs for confidentiality APIs. It
// defaults to `MODIFIED` (catch-all) for the browser-side where
// filtering has already occurred in the renderer.
TelemetrySignalType GetTelemetrySignalType(
    const std::string& api_name,
    const base::ListValue& args_unsafe,
    DomActionType::Type action_type = DomActionType::MODIFIED);

// Returns true if the telemetry signal is considered high-risk.
// Primarily used by the Renderer-side delegate to decide if the
// corresponding extension action should be forwarded to the browser.
bool IsHighRiskEvent(TelemetrySignalType signal_type);

// Normalizes a raw argument list for telemetry by applying
// API-specific rules. (e.g., dropping the 'old value' at index 2 for
// blinkSetAttribute).
std::vector<std::string> GetArgumentsList(const std::string& api_name,
                                          const base::ListValue& args_unsafe);

}  // namespace extensions::activity_log_policy_util

#endif  // CHROME_COMMON_EXTENSIONS_ACTIVITY_LOG_POLICY_UTIL_H_
