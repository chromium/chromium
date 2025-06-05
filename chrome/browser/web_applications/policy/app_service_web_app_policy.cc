// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/policy/app_service_web_app_policy.h"

#include <algorithm>
#include <string_view>

#include "ash/constants/web_app_id_constants.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"

namespace web_app {

namespace {

enum class SystemWebAppType;

// This mapping excludes SWAs not included in official builds (like SAMPLE).
// These app Id constants need to be kept in sync with java/com/
// google/chrome/cros/policyconverter/ChromePolicySettingsProcessor.java
// LINT.IfChange
constexpr auto kSystemWebAppsMapping =
    base::MakeFixedFlatMap<std::string_view, ash::SystemWebAppType>(
        {{"file_manager", ash::SystemWebAppType::FILE_MANAGER},
         {"settings", ash::SystemWebAppType::SETTINGS},
         {"camera", ash::SystemWebAppType::CAMERA},
         {"terminal", ash::SystemWebAppType::TERMINAL},
         {"media", ash::SystemWebAppType::MEDIA},
         {"help", ash::SystemWebAppType::HELP},
         {"print_management", ash::SystemWebAppType::PRINT_MANAGEMENT},
         {"scanning", ash::SystemWebAppType::SCANNING},
         {"diagnostics", ash::SystemWebAppType::DIAGNOSTICS},
         {"connectivity_diagnostics",
          ash::SystemWebAppType::CONNECTIVITY_DIAGNOSTICS},
         {"eche", ash::SystemWebAppType::ECHE},
         {"crosh", ash::SystemWebAppType::CROSH},
         {"personalization", ash::SystemWebAppType::PERSONALIZATION},
         {"shortcut_customization",
          ash::SystemWebAppType::SHORTCUT_CUSTOMIZATION},
         {"shimless_rma", ash::SystemWebAppType::SHIMLESS_RMA},
         {"demo_mode", ash::SystemWebAppType::DEMO_MODE},
         {"os_feedback", ash::SystemWebAppType::OS_FEEDBACK},
         {"os_sanitize", ash::SystemWebAppType::OS_SANITIZE},
         {"projector", ash::SystemWebAppType::PROJECTOR},
         {"firmware_update", ash::SystemWebAppType::FIRMWARE_UPDATE},
         {"os_flags", ash::SystemWebAppType::OS_FLAGS},
         {"vc_background", ash::SystemWebAppType::VC_BACKGROUND},
         {"print_preview_cros", ash::SystemWebAppType::PRINT_PREVIEW_CROS},
         {"boca", ash::SystemWebAppType::BOCA},
         {"app_mall", ash::SystemWebAppType::MALL},
         {"recorder", ash::SystemWebAppType::RECORDER},
         {"graduation", ash::SystemWebAppType::GRADUATION}});
// LINT.ThenChange(//depot/google3/java/com/google/chrome/cros/policyconverter/ChromePolicySettingsProcessor.java)

constexpr ash::SystemWebAppType GetMaxSystemWebAppType() {
  return std::ranges::max_element(
             kSystemWebAppsMapping, std::ranges::less{},
             &decltype(kSystemWebAppsMapping)::value_type::second)
      ->second;
}

static_assert(GetMaxSystemWebAppType() == ash::SystemWebAppType::kMaxValue,
              "Not all SWA types are listed in |system_web_apps_mapping|.");

}  // namespace

std::optional<std::string_view> GetPolicyIdForSystemWebAppType(
    ash::SystemWebAppType swa_type) {
  for (const auto& [policy_id, mapped_swa_type] : kSystemWebAppsMapping) {
    if (mapped_swa_type == swa_type) {
      return policy_id;
    }
  }
  return {};
}

bool IsArcAppPolicyId(std::string_view policy_id) {
  return base::Contains(policy_id, '.') &&
         !WebAppPolicyManager::IsWebAppPolicyId(policy_id);
}

bool IsSystemWebAppPolicyId(std::string_view policy_id) {
  return base::Contains(kSystemWebAppsMapping, policy_id);
}

}  // namespace web_app
