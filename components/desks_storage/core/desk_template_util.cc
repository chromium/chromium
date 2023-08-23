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

bool AreDeskTemplatesEqual(const ash::DeskTemplate* template_one,
                           const ash::DeskTemplate* template_two) {
  // confirm metadata is equal.
  if (template_one->uuid() != template_two->uuid() ||
      template_one->source() != template_two->source() ||
      template_one->created_time() != template_two->created_time() ||
      template_one->GetLastUpdatedTime() !=
          template_two->GetLastUpdatedTime() ||
      template_one->should_launch_on_startup() !=
          template_two->should_launch_on_startup()) {
    return false;
  }

  if (template_one->uuid() != template_two->uuid() ||
      template_one->source() != template_two->source() ||
      template_one->created_time() != template_two->created_time() ||
      template_one->GetLastUpdatedTime() !=
          template_two->GetLastUpdatedTime()) {
    return false;
  }

  const auto* restore_data_one = template_one->desk_restore_data();
  const auto* restore_data_two = template_two->desk_restore_data();

  // iterate over each app, confirm its in the other's list.
  for (const auto& launch_list_one :
       restore_data_one->app_id_to_launch_list()) {
    const auto& launch_list_two_iter =
        restore_data_two->app_id_to_launch_list().find(launch_list_one.first);
    if (launch_list_two_iter ==
        restore_data_two->app_id_to_launch_list().end()) {
      return false;
    }
    const auto& launch_list_two_app = launch_list_two_iter->second;

    // iterate over each window, confirm its in the other's list.
    for (const auto& restore_window_one : launch_list_one.second) {
      const auto& restore_window_two_iter =
          launch_list_two_app.find(restore_window_one.first);
      if (restore_window_two_iter == launch_list_two_app.end()) {
        return false;
      }

      // Compare app restore data structs.
      if (*restore_window_one.second != *restore_window_two_iter->second) {
        return false;
      }
    }
  }

  return true;
}

}  // namespace desk_template_util

}  // namespace desks_storage
