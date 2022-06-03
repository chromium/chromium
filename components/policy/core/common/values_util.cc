// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/values_util.h"

#include <vector>

namespace policy {

base::flat_set<std::string> ValueToStringSet(const base::Value* value) {
  if (!value)
    return base::flat_set<std::string>();

  if (!value->is_list())
    return base::flat_set<std::string>();

  const auto& items = value->GetList();

  std::vector<std::string> item_vector;
  item_vector.reserve(items.size());

  for (const auto& item : items) {
    if (item.is_string())
      item_vector.emplace_back(item.GetString());
  }

  return base::flat_set<std::string>(std::move(item_vector));
}

}  // namespace policy
