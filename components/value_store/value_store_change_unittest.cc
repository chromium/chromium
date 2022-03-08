// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/value_store/value_store_change.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::DictionaryValue;
using base::Value;

namespace value_store {

TEST(ValueStoreChangeTest, NullOldValue) {
  ValueStoreChange change("key", absl::nullopt, base::Value("value"));

  EXPECT_EQ("key", change.key());
  EXPECT_EQ(nullptr, change.old_value());
  {
    base::Value expected("value");
    EXPECT_EQ(*change.new_value(), expected);
  }
}

TEST(ValueStoreChangeTest, NullNewValue) {
  ValueStoreChange change("key", base::Value("value"), absl::nullopt);

  EXPECT_EQ("key", change.key());
  {
    base::Value expected("value");
    EXPECT_EQ(*change.old_value(), expected);
  }
  EXPECT_EQ(nullptr, change.new_value());
}

TEST(ValueStoreChangeTest, NonNullValues) {
  ValueStoreChange change("key", base::Value("old_value"),
                          base::Value("new_value"));

  EXPECT_EQ("key", change.key());
  {
    base::Value expected("old_value");
    EXPECT_EQ(*change.old_value(), expected);
  }
  {
    base::Value expected("new_value");
    EXPECT_EQ(*change.new_value(), expected);
  }
}

TEST(ValueStoreChangeTest, ToValue) {
  // Create a mildly complicated structure that has dots in it.
  base::Value inner_dict(base::Value::Type::DICTIONARY);
  inner_dict.SetKey("you", base::Value("nodots"));

  base::Value value(base::Value::Type::DICTIONARY);
  value.SetKey("key", base::Value("value"));
  value.SetKey("key.with.dots", base::Value("value.with.dots"));
  value.SetKey("tricked", std::move(inner_dict));
  value.SetKey("tricked.you", base::Value("with.dots"));

  ValueStoreChangeList change_list;
  change_list.push_back(ValueStoreChange("key", value.Clone(), value.Clone()));
  change_list.push_back(
      ValueStoreChange("key.with.dots", value.Clone(), value.Clone()));

  base::Value changes_value = ValueStoreChange::ToValue(std::move(change_list));

  base::Value v1(value.Clone());
  base::Value v2(value.Clone());
  base::Value v3(value.Clone());
  base::Value v4(value.Clone());

  base::Value inner_dict2(base::Value::Type::DICTIONARY);
  base::Value inner_dict3(base::Value::Type::DICTIONARY);

  inner_dict2.SetKey("oldValue", std::move(v1));
  inner_dict2.SetKey("newValue", std::move(v2));
  inner_dict3.SetKey("oldValue", std::move(v3));
  inner_dict3.SetKey("newValue", std::move(v4));

  base::Value expected_from_json(base::Value::Type::DICTIONARY);
  expected_from_json.SetKey("key", std::move(inner_dict2));
  expected_from_json.SetKey("key.with.dots", std::move(inner_dict3));
  EXPECT_EQ(changes_value, expected_from_json);
}

}  // namespace value_store
