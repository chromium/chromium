// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/resolvers/geolocation_permission_resolver.h"

#include <optional>
#include <variant>

#include "base/debug/stack_trace.h"
#include "base/notimplemented.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/resolvers/permission_prompt_options.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace {
content::PermissionStatus GeolocationOptionToPermissionStatus(
    PermissionOption option) {
  switch (option) {
    case PermissionOption::kAsk:
      return content::PermissionStatus::ASK;
    case PermissionOption::kAllowed:
      return content::PermissionStatus::GRANTED;
    case PermissionOption::kDenied:
      return content::PermissionStatus::DENIED;
  }
}
}  // namespace

namespace permissions {

GeolocationPermissionResolver::GeolocationPermissionResolver(
    bool requested_precise)
    : PermissionResolver(ContentSettingsType::GEOLOCATION_WITH_OPTIONS),
      requested_precise_(requested_precise) {}

blink::mojom::PermissionStatus
GeolocationPermissionResolver::DeterminePermissionStatus(
    const PermissionSetting& setting) const {
  GeolocationSetting geo_setting = std::get<GeolocationSetting>(setting);

  return requested_precise_
             ? GeolocationOptionToPermissionStatus(geo_setting.precise)
             : GeolocationOptionToPermissionStatus(geo_setting.approximate);
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
        } else {
          setting.precise = PermissionOption::kAllowed;
        }
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
