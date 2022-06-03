// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/values_util.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

TEST(PolicyValuesToStringSetTest, Convert) {
  base::Value::ListStorage items;
  items.push_back(base::Value("1"));
  items.push_back(base::Value("2"));
  items.push_back(base::Value("3"));
  base::Value value(items);
  base::flat_set<std::string> expected_set = {"1", "2", "3"};
  EXPECT_EQ(expected_set, ValueToStringSet(&value));
}

TEST(PolicyValuesToStringSetTest, SkipInvalidItem) {
  base::Value::ListStorage items;
  items.push_back(base::Value("1"));
  items.push_back(base::Value());
  items.push_back(base::Value(0));
  items.push_back(base::Value(true));
  items.push_back(base::Value(base::Value::Type::BINARY));
  items.push_back(base::Value(base::Value::Type::LIST));
  items.push_back(base::Value(base::Value::Type::DICTIONARY));
  items.push_back(base::Value("2"));
  items.push_back(base::Value("3"));
  items.push_back(base::Value(""));
  base::Value value(items);
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
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY),
      std::make_unique<base::Value>(""),
      std::make_unique<base::Value>("a"),
      std::make_unique<base::Value>("0"),
  };
  for (const auto& value : values)
    EXPECT_EQ(base::flat_set<std::string>(), ValueToStringSet(value.get()));
}

}  // namespace policy
