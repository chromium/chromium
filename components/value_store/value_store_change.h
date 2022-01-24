// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VALUE_STORE_VALUE_STORE_CHANGE_H_
#define COMPONENTS_VALUE_STORE_VALUE_STORE_CHANGE_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace value_store {

class ValueStoreChange;
typedef std::vector<ValueStoreChange> ValueStoreChangeList;

// A change to a setting.
class ValueStoreChange {
 public:
  // Converts an ValueStoreChangeList into base::Value of the form:
  // { "foo": { "key": "foo", "oldValue": "bar", "newValue": "baz" } }
  static base::Value ToValue(ValueStoreChangeList changes);

  ValueStoreChange(const std::string& key,
                   absl::optional<base::Value> old_value,
                   absl::optional<base::Value> new_value);

  ValueStoreChange(const ValueStoreChange& other) = delete;
  ValueStoreChange(ValueStoreChange&& other);
  ValueStoreChange& operator=(const ValueStoreChange& other) = delete;
  ValueStoreChange& operator=(ValueStoreChange&& other);

  ~ValueStoreChange();

  // Gets the key of the setting which changed.
  const std::string& key() const { return key_; }

  // Gets the value of the setting before the change, or NULL if there was no
  // old value.
  const base::Value* old_value() const;

  // Gets the value of the setting after the change, or NULL if there is no new
  // value.
  const base::Value* new_value() const;

 private:
  std::string key_;
  absl::optional<base::Value> old_value_;
  absl::optional<base::Value> new_value_;
};

}  // namespace value_store

#endif  // COMPONENTS_VALUE_STORE_VALUE_STORE_CHANGE_H_
