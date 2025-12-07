// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_POLICY_APP_SERVICE_WEB_APP_POLICY_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_POLICY_APP_SERVICE_WEB_APP_POLICY_H_

#include <optional>
#include <string_view>

#include "ash/webui/system_apps/public/system_web_app_type.h"

namespace web_app {

// Returns the policy ID (string_view) for a given System Web App Type.
// Returns std::nullopt if the type is not found in the mapping.
std::optional<std::string_view> GetPolicyIdForSystemWebAppType(
    ash::SystemWebAppType swa_type);

// Checks if the given policy ID corresponds to a System Web App.
bool IsSystemWebAppPolicyId(std::string_view policy_id);

// Checks whether |policy_id| specifies an Arc App.
bool IsArcAppPolicyId(std::string_view policy_id);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_POLICY_APP_SERVICE_WEB_APP_POLICY_H_
