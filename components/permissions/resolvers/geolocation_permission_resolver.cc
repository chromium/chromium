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
#include "components/permissions/permission_prompt_decision.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/resolvers/permission_prompt_options.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-forward.h"

namespace permissions {

namespace {

blink::mojom::PermissionStatus PermissionOptionToPermissionStatus(
    PermissionOption permission_option) {
  switch (permission_option) {
    case PermissionOption::kAllowed:
      return blink::mojom::PermissionStatus::GRANTED;
    case PermissionOption::kDenied:
      return blink::mojom::PermissionStatus::DENIED;
    case PermissionOption::kAsk:
      return blink::mojom::PermissionStatus::ASK;
  }
  NOTREACHED();
}

}  // namespace

GeolocationPermissionResolver::GeolocationPermissionResolver(
    const blink::mojom::PermissionDescriptor& permission_descriptor)
    : PermissionResolver(ContentSettingsType::GEOLOCATION_WITH_OPTIONS) {
  if (permission_descriptor.name == blink::mojom::PermissionName::GEOLOCATION) {
    requested_precise_ = true;
  } else if (permission_descriptor.name ==
             blink::mojom::PermissionName::GEOLOCATION_APPROXIMATE) {
    requested_precise_ = false;
  } else {
    NOTREACHED();
  }
}

blink::mojom::PermissionStatus
GeolocationPermissionResolver::DeterminePermissionStatus(
    const PermissionSetting& setting) const {
  GeolocationSetting geo_setting = std::get<GeolocationSetting>(setting);
  blink::mojom::PermissionStatus precise_status =
      PermissionOptionToPermissionStatus(geo_setting.precise);
  if (requested_precise_ &&
      precise_status != blink::mojom::PermissionStatus::DENIED) {
    return precise_status;
  }
  return PermissionOptionToPermissionStatus(geo_setting.approximate);
}

PermissionSetting
GeolocationPermissionResolver::ComputePermissionDecisionResult(
    const PermissionSetting& previous_setting,
    const PermissionPromptDecision& decision) const {
  auto setting = std::get<GeolocationSetting>(previous_setting);

  switch (decision.overall_decision) {
    case PermissionDecision::kAllow:
    case PermissionDecision::kAllowThisTime:
      setting.approximate = PermissionOption::kAllowed;

      if (requested_precise_) {
        CHECK(std::holds_alternative<GeolocationPromptOptions>(
            decision.prompt_options));
        switch (std::get<GeolocationPromptOptions>(decision.prompt_options)
                    .selected_accuracy) {
          case GeolocationAccuracy::kPrecise:
            setting.precise = PermissionOption::kAllowed;
            break;
          case GeolocationAccuracy::kApproximate:
            // If the user downgraded the request, we consider precise as
            // blocked.
            setting.precise = PermissionOption::kDenied;
            break;
        }
      } else {
        CHECK(std::holds_alternative<std::monostate>(decision.prompt_options) ||
              std::get<GeolocationPromptOptions>(decision.prompt_options)
                      .selected_accuracy == GeolocationAccuracy::kApproximate);
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
