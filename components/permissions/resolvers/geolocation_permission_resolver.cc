// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/resolvers/geolocation_permission_resolver.h"

#include <optional>
#include <variant>

#include "base/debug/stack_trace.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/resolvers/permission_prompt_options.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace permissions {

GeolocationPermissionResolver::GeolocationPermissionResolver(
    bool requested_precise)
    : PermissionResolver(ContentSettingsType::GEOLOCATION_WITH_OPTIONS),
      requested_precise_(requested_precise) {}

blink::mojom::PermissionStatus
GeolocationPermissionResolver::DeterminePermissionStatus(
    const PermissionSetting& setting) const {
  GeolocationSetting geo_setting = std::get<GeolocationSetting>(setting);
  if (geo_setting.precise == PermissionOption::kAllowed) {
    return blink::mojom::PermissionStatus::GRANTED;
  }
  switch (geo_setting.approximate) {
    case PermissionOption::kAllowed:
      return blink::mojom::PermissionStatus::GRANTED;
    case PermissionOption::kDenied:
      return blink::mojom::PermissionStatus::DENIED;
    case PermissionOption::kAsk:
      return blink::mojom::PermissionStatus::ASK;
  }
  NOTREACHED();
}

PermissionSetting
GeolocationPermissionResolver::ComputePermissionDecisionResult(
    const PermissionSetting& previous_setting,
    PermissionDecision decision,
    PromptOptions prompt_options) const {
  CHECK(requested_precise_ || std::get_if<std::monostate>(&prompt_options));
  auto setting = std::get<GeolocationSetting>(previous_setting);

  switch (decision) {
    case PermissionDecision::kAllow:
    case PermissionDecision::kAllowThisTime:
      setting.approximate = PermissionOption::kAllowed;

      if (requested_precise_) {
        if (auto* geo_options =
                std::get_if<GeolocationPromptOptions>(&prompt_options)) {
          // If the user downgraded the request, we consider precise as blocked.
          setting.precise = geo_options->selected_precise
                                ? PermissionOption::kAllowed
                                : PermissionOption::kDenied;
        }
        // If the prompt_options are not set it means that this did not go
        // through a prompt, so let's just keep the value in previous setting.
        //
        // TODO(https://crbug.com/450752868): This implicit logic is fragile.
        // Find out how to improve this.
      }
      break;
    case PermissionDecision::kNone:
      break;
    case PermissionDecision::kDeny:
      setting.approximate = PermissionOption::kDenied;
      setting.precise = PermissionOption::kDenied;
  }

  return setting;
}

GeolocationPermissionResolver::PromptParameters
GeolocationPermissionResolver::GetPromptParameters(
    const PermissionSetting& current_setting_state) const {
  NOTIMPLEMENTED();
  return PromptParameters();
}

}  // namespace permissions
