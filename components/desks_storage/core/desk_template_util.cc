// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_template_util.h"

#include "base/ranges/algorithm.h"

namespace desks_storage {

namespace desk_template_util {

ash::DeskTemplate* FindOtherEntryWithName(
    const std::u16string& name,
    const base::Uuid& uuid,
    const base::flat_map<base::Uuid, std::unique_ptr<ash::DeskTemplate>>&
        entries) {
  auto iter = base::ranges::find_if(
      entries,
      [name, uuid](const std::pair<base::Uuid,
                                   std::unique_ptr<ash::DeskTemplate>>& entry) {
        // Name duplication is allowed if one of the templates is an admin
        // template or if it's a floating workspace template.
        return (
            entry.second->uuid() != uuid &&
            entry.second->template_name() == name &&
            entry.second->source() != ash::DeskTemplateSource::kPolicy &&
            (entry.second->type() == ash::DeskTemplateType::kTemplate ||
             entry.second->type() == ash::DeskTemplateType::kSaveAndRecall));
      });
  if (iter == entries.end()) {
    return nullptr;
  }
  return iter->second.get();
}

}  // namespace desk_template_util

}  // namespace desks_storage
