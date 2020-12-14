// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "components/full_restore/restore_data.h"

#include "base/values.h"
#include "components/full_restore/app_launch_info.h"

namespace full_restore {

RestoreData::RestoreData() = default;
RestoreData::~RestoreData() = default;

base::Value RestoreData::ConvertToValue() const {
  base::Value restore_data_list(base::Value::Type::LIST);
  for (const auto& it : app_id_to_launch_list_) {
    if (it.second.empty())
      continue;

    base::Value launch_list(base::Value::Type::LIST);
    for (const auto& data : it.second) {
      base::Value info_dict(base::Value::Type::DICTIONARY);
      info_dict.SetKey(base::NumberToString(data.first),
                       data.second->ConvertToValue());
      launch_list.Append(std::move(info_dict));
    }

    base::Value restore_data_dict(base::Value::Type::DICTIONARY);
    restore_data_dict.SetKey(it.first, std::move(launch_list));
    restore_data_list.Append(std::move(restore_data_dict));
  }
  return restore_data_list;
}

void RestoreData::AddAppLaunchInfo(
    std::unique_ptr<AppLaunchInfo> app_launch_info) {
  if (!app_launch_info || !app_launch_info->id.has_value())
    return;

  const std::string app_id = app_launch_info->app_id;
  const int32_t id = app_launch_info->id.value();
  app_id_to_launch_list_[app_id][id] =
      std::make_unique<AppRestoreData>(std::move(app_launch_info));
}

}  // namespace full_restore
