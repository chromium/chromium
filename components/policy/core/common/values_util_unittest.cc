// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/values_util.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

TEST(PolicyValuesToStringSetTest, Convert) {
  base::Value::List items;
  items.Append("1");
  items.Append("2");
  items.Append("3");
  base::Value value(std::move(items));
  base::flat_set<std::string> expected_set = {"1", "2", "3"};
  EXPECT_EQ(expected_set, ValueToStringSet(&value));
}

TEST(PolicyValuesToStringSetTest, SkipInvalidItem) {
  base::Value::List items;
  items.Append("1");
  items.Append(base::Value());
  items.Append(0);
  items.Append(true);
  items.Append(base::Value(base::Value::Type::BINARY));
  items.Append(base::Value::List());
  items.Append(base::Value::Dict());
  items.Append("2");
  items.Append("3");
  items.Append("");
  base::Value value(std::move(items));
  base::flat_set<std::string> expected_set = {"1", "2", "3", ""};
  EXPECT_EQ(expected_set, ValueToStringSet(&value));
}

TEST(PolicyValuesToStringSetTest, InvalidValues) {
  std::unique_ptr<base::Value> values[] = {
      nullptr,
      std::make_unique<base::Value>(),
      std::make_unique<base::Value>(0),
      std::make_unique<base::Value>(true),
      std::make_unique<base::Value>(base::Value::Type::BINARY),
      std::make_unique<base::Value>(base::Value::Type::LIST),
      std::make_unique<base::Value>(base::Value::Type::DICT),
      std::make_unique<base::Value>(""),
      std::make_unique<base::Value>("a"),
      std::make_unique<base::Value>("0"),
  };
  for (const auto& value : values)
    EXPECT_EQ(base::flat_set<std::string>(), ValueToStringSet(value.get()));
}

}  // namespace policy
