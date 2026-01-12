// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/values_util.h"

#include <vector>

namespace policy {

base::flat_set<std::string> ValueToStringSet(const base::Value* value) {
  if (!value || !value->is_list())
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

ComponentPolicyMap CopyComponentPolicyMap(const ComponentPolicyMap& map) {
  ComponentPolicyMap new_map;
  for (const auto& [policy_namespace, value] : map) {
    new_map[policy_namespace] = value.Clone();
  }
  return new_map;
}

}  // namespace policy
