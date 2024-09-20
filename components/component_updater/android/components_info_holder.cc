// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/android/components_info_holder.h"

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/version.h"
#include "components/component_updater/component_updater_service.h"

namespace component_updater {

ComponentsInfoHolder::ComponentsInfoHolder() = default;
ComponentsInfoHolder::~ComponentsInfoHolder() = default;

ComponentsInfoHolder* ComponentsInfoHolder::GetInstance() {
  static base::NoDestructor<ComponentsInfoHolder> holder;
  return holder.get();
}

void ComponentsInfoHolder::AddComponent(const std::string& component_id,
                                        const base::Version& version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  components_[component_id] = version;
}

std::vector<ComponentInfo> ComponentsInfoHolder::GetComponents() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<ComponentInfo> components_info;
  for (const auto& [component_id, registration] : components_) {
    components_info.emplace_back(component_id, "", u"", registration, "");
  }
  return components_info;
}

}  // namespace component_updater
