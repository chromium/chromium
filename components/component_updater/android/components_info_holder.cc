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
                                        const base::Version& version,
                                        const std::string& cohort_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& component : components_) {
    if (component.id == component_id) {
      // Update the existing entry
      component.version = version;
      component.cohort_id = cohort_id;
      return;
    }
  }

  // Add a new entry if not found
  components_.emplace_back(component_id, "", std::u16string(), version,
                           cohort_id);
}

std::vector<ComponentInfo> ComponentsInfoHolder::GetComponents() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return components_;
}

}  // namespace component_updater
