// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VALUE_STORE_VALUE_STORE_CHANGE_H_
#define COMPONENTS_VALUE_STORE_VALUE_STORE_CHANGE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/values.h"

namespace value_store {

struct ValueStoreChange;
typedef std::vector<ValueStoreChange> ValueStoreChangeList;

// A change to a setting.
struct ValueStoreChange {
  // Converts an ValueStoreChangeList into base::Value of the form:
  // { "foo": { "key": "foo", "oldValue": "bar", "newValue": "baz" } }
  static base::Value ToValue(ValueStoreChangeList changes);

  ValueStoreChange(const std::string& key,
                   std::optional<base::Value> old_value,
                   std::optional<base::Value> new_value);

  ValueStoreChange(const ValueStoreChange& other) = delete;
  ValueStoreChange(ValueStoreChange&& other);
  ValueStoreChange& operator=(const ValueStoreChange& other) = delete;
  ValueStoreChange& operator=(ValueStoreChange&& other);

  ~ValueStoreChange();

  std::string key;
  std::optional<base::Value> old_value;
  std::optional<base::Value> new_value;
};

}  // namespace value_store

#endif  // COMPONENTS_VALUE_STORE_VALUE_STORE_CHANGE_H_
