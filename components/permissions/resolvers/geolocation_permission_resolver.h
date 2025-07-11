// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_RESOLVERS_GEOLOCATION_PERMISSION_RESOLVER_H_
#define COMPONENTS_PERMISSIONS_RESOLVERS_GEOLOCATION_PERMISSION_RESOLVER_H_

#include "components/permissions/resolvers/permission_resolver.h"

namespace permissions {

// A |PermissionResolver| for the geolocation permission supporting
// approximate/precise location requests.
class GeolocationPermissionResolver : public PermissionResolver {
 public:
  explicit GeolocationPermissionResolver(bool requested_precise);

  blink::mojom::PermissionStatus DeterminePermissionStatus(
      const PermissionSetting& setting) const override;

  PermissionSetting ComputePermissionDecisionResult(
      const PermissionSetting& previous_setting,
      PermissionDecision decision,
      PromptOptions prompt_options) const override;

  PromptParameters GetPromptParameters(
      const PermissionSetting& current_setting_state) const override;

 private:
  bool requested_precise_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_RESOLVERS_GEOLOCATION_PERMISSION_RESOLVER_H_
