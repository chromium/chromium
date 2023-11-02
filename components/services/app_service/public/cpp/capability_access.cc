// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/capability_access.h"

#include "components/services/app_service/public/cpp/app_types.h"

namespace apps {

CapabilityAccess::CapabilityAccess(const std::string& app_id)
    : app_id(app_id) {}

CapabilityAccess::~CapabilityAccess() = default;

CapabilityAccessPtr CapabilityAccess::Clone() const {
  auto capability_access = std::make_unique<CapabilityAccess>(app_id);

  capability_access->camera = camera;
  capability_access->microphone = microphone;
  return capability_access;
}

CapabilityAccessPtr ConvertMojomCapabilityAccessToCapabilityAccess(
    const apps::mojom::CapabilityAccessPtr& mojom_capability_access) {
  if (!mojom_capability_access)
    return nullptr;

  auto capability_access =
      std::make_unique<CapabilityAccess>(mojom_capability_access->app_id);
  capability_access->camera = GetOptionalBool(mojom_capability_access->camera);
  capability_access->microphone =
      GetOptionalBool(mojom_capability_access->microphone);
  return capability_access;
}

apps::mojom::CapabilityAccessPtr ConvertCapabilityAccessToMojomCapabilityAccess(
    const CapabilityAccessPtr& capability_access) {
  auto mojom_capability_access = apps::mojom::CapabilityAccess::New();
  if (!capability_access) {
    return mojom_capability_access;
  }

  mojom_capability_access->app_id = capability_access->app_id;
  mojom_capability_access->camera =
      GetMojomOptionalBool(capability_access->camera);
  mojom_capability_access->microphone =
      GetMojomOptionalBool(capability_access->microphone);
  return mojom_capability_access;
}

std::vector<CapabilityAccessPtr>
ConvertMojomCapabilityAccessesToCapabilityAccesses(
    const std::vector<apps::mojom::CapabilityAccessPtr>&
        mojom_capability_accesses) {
  std::vector<CapabilityAccessPtr> ret;
  ret.reserve(mojom_capability_accesses.size());
  for (const auto& mojom_capability_access : mojom_capability_accesses) {
    ret.push_back(ConvertMojomCapabilityAccessToCapabilityAccess(
        mojom_capability_access));
  }
  return ret;
}

std::vector<apps::mojom::CapabilityAccessPtr>
ConvertCapabilityAccessesToMojomCapabilityAccesses(
    const std::vector<CapabilityAccessPtr>& capability_accesses) {
  std::vector<apps::mojom::CapabilityAccessPtr> ret;
  ret.reserve(capability_accesses.size());
  for (const auto& capability_access : capability_accesses) {
    ret.push_back(
        ConvertCapabilityAccessToMojomCapabilityAccess(capability_access));
  }
  return ret;
}

}  // namespace apps
