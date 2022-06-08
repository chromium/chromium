// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_template_util.h"

namespace desks_storage {

namespace desk_template_util {

ash::DeskTemplate* FindOtherEntryWithName(
    const std::u16string& name,
    const base::GUID& uuid,
    const std::map<base::GUID, std::unique_ptr<ash::DeskTemplate>>& entries) {
  auto iter = std::find_if(
      entries.begin(), entries.end(),
      [name, uuid](const std::pair<const base::GUID,
                                   std::unique_ptr<ash::DeskTemplate>>& entry) {
        // Name duplication is allowed if one of the templates is an admin
        // template.
        return (entry.second->uuid() != uuid &&
                entry.second->template_name() == name &&
                entry.second->source() != ash::DeskTemplateSource::kPolicy);
      });
  if (iter == entries.end()) {
    return nullptr;
  }
  return iter->second.get();
}

}  // namespace desk_template_util

}  // namespace desks_storage
