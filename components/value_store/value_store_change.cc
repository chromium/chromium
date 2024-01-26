// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/value_store/value_store_change.h"

#include <utility>

#include "base/check.h"
#include "base/json/json_writer.h"
#include "base/stl_util.h"

namespace value_store {

base::Value ValueStoreChange::ToValue(ValueStoreChangeList changes) {
  base::Value::Dict changes_dict;
  for (auto& change : changes) {
    base::Value::Dict change_dict;
    if (change.old_value) {
      change_dict.Set("oldValue", std::move(*change.old_value));
    }
    if (change.new_value) {
      change_dict.Set("newValue", std::move(*change.new_value));
    }
    changes_dict.Set(change.key, std::move(change_dict));
  }
  return base::Value(std::move(changes_dict));
}

ValueStoreChange::ValueStoreChange(const std::string& key,
                                   std::optional<base::Value> old_value,
                                   std::optional<base::Value> new_value)
    : key(key),
      old_value(std::move(old_value)),
      new_value(std::move(new_value)) {}

ValueStoreChange::~ValueStoreChange() = default;

ValueStoreChange::ValueStoreChange(ValueStoreChange&& other) = default;
ValueStoreChange& ValueStoreChange::operator=(ValueStoreChange&& other) =
    default;

}  // namespace value_store
