// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/values_util.h"

#include <functional>
#include <vector>

#include "base/hash/hash.h"

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

size_t PolicyValueHash(const base::Value& value) {
  // Use type_val with the hash to make sure 0.0, 0 and False map to different
  // hashes.
  size_t type_val = static_cast<size_t>(value.type());

  switch (value.type()) {
    case base::Value::Type::DICT: {
      // Mix type_val with the original seed.
      size_t hash = base::HashInts(type_val, 0x3e530635677611c0ULL);
      const base::Value::Dict& policy_dict = value.GetDict();
      for (const auto [key, dict_val] : policy_dict) {
        hash = base::HashInts(hash, base::FastHash(key));
        hash = base::HashInts(hash, PolicyValueHash(dict_val));
      }
      return hash;
    }
    case base::Value::Type::LIST: {
      // Mix type_val with the original seed.
      size_t hash = base::HashInts(type_val, 0x6d2c1860e6981440ULL);
      for (const auto& element : value.GetList()) {
        hash = base::HashInts(hash, PolicyValueHash(element));
      }
      return hash;
    }
    case base::Value::Type::NONE:
      // Mix type_val with the original seed.
      return base::HashInts(type_val, 0x37e23a48e89139d0ULL);
    case base::Value::Type::BOOLEAN:
      return base::HashInts(type_val, std::hash<bool>()(value.GetBool()));
    case base::Value::Type::INTEGER:
      return base::HashInts(type_val, std::hash<int>()(value.GetInt()));
    case base::Value::Type::DOUBLE:
      return base::HashInts(type_val, std::hash<double>()(value.GetDouble()));
    case base::Value::Type::STRING:
      // Mix type_val with the string hash.
      return base::HashInts(type_val, base::FastHash(value.GetString()));
    default:
      // Handle any other types, mixing in a default seed.
      return base::HashInts(type_val, 0x4f4dfa4e212c8b1ULL);
  }
}

}  // namespace policy
