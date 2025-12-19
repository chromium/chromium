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

TEST(PolicyValueHashTest, Hash) {
  // Test simple types.
  EXPECT_NE(PolicyValueHash(base::Value(true)),
            PolicyValueHash(base::Value(false)));
  EXPECT_EQ(PolicyValueHash(base::Value(true)),
            PolicyValueHash(base::Value(true)));

  EXPECT_NE(PolicyValueHash(base::Value(0)), PolicyValueHash(base::Value(1)));
  EXPECT_EQ(PolicyValueHash(base::Value(1)), PolicyValueHash(base::Value(1)));

  EXPECT_NE(PolicyValueHash(base::Value(1.0)),
            PolicyValueHash(base::Value(2.0)));
  EXPECT_EQ(PolicyValueHash(base::Value(1.0)),
            PolicyValueHash(base::Value(1.0)));

  EXPECT_NE(PolicyValueHash(base::Value("foo")),
            PolicyValueHash(base::Value("bar")));
  EXPECT_EQ(PolicyValueHash(base::Value("foo")),
            PolicyValueHash(base::Value("foo")));

  // Test lists.
  base::Value::List list1;
  list1.Append(1);
  list1.Append("foo");

  base::Value::List list2;
  list2.Append(1);
  list2.Append("foo");

  base::Value::List list3;
  list3.Append(1);
  list3.Append("bar");

  EXPECT_EQ(PolicyValueHash(base::Value(std::move(list1))),
            PolicyValueHash(base::Value(list2.Clone())));
  EXPECT_NE(PolicyValueHash(base::Value(std::move(list2))),
            PolicyValueHash(base::Value(std::move(list3))));

  base::Value::List list4;
  list4.Append(1);
  list4.Append("foo");

  base::Value::List list5;
  list5.Append("foo");
  list5.Append(1);

  EXPECT_NE(PolicyValueHash(base::Value(std::move(list4))),
            PolicyValueHash(base::Value(std::move(list5))));

  // Test dictionaries.
  base::Value::Dict dict1;
  dict1.Set("foo", 1);
  dict1.Set("bar", "baz");

  base::Value::Dict dict2;
  dict2.Set("foo", 1);
  dict2.Set("bar", "baz");

  base::Value::Dict dict3;
  dict3.Set("foo", 1);
  dict3.Set("bar", "qux");

  base::Value::Dict dict4;
  dict4.Set("foo", 1);

  base::Value::Dict dict5;
  dict5.Set("bar", 1);

  EXPECT_EQ(PolicyValueHash(base::Value(std::move(dict1))),
            PolicyValueHash(base::Value(dict2.Clone())));
  EXPECT_NE(PolicyValueHash(base::Value(std::move(dict2))),
            PolicyValueHash(base::Value(std::move(dict3))));
  EXPECT_NE(PolicyValueHash(base::Value(std::move(dict4))),
            PolicyValueHash(base::Value(std::move(dict5))));

  // Test nested values.
  base::Value::List nested_list1;
  base::Value::Dict nested_dict1;
  nested_dict1.Set("key", "value");
  nested_list1.Append(std::move(nested_dict1));

  base::Value::List nested_list2;
  base::Value::Dict nested_dict2;
  nested_dict2.Set("key", "value");
  nested_list2.Append(std::move(nested_dict2));

  base::Value::List nested_list3;
  base::Value::Dict nested_dict3;
  nested_dict3.Set("key", "other_value");
  nested_list3.Append(std::move(nested_dict3));

  EXPECT_EQ(PolicyValueHash(base::Value(std::move(nested_list1))),
            PolicyValueHash(base::Value(nested_list2.Clone())));
  EXPECT_NE(PolicyValueHash(base::Value(std::move(nested_list2))),
            PolicyValueHash(base::Value(std::move(nested_list3))));

  // Test distinct hashes for different empty/zero values.
  base::Value::List distinct_values;
  distinct_values.Append(0);
  distinct_values.Append(0.0);
  distinct_values.Append(base::Value());
  distinct_values.Append("");
  distinct_values.Append("0");
  distinct_values.Append(base::Value(base::Value::Type::DICT));
  distinct_values.Append(base::Value(base::Value::Type::LIST));
  distinct_values.Append(false);

  for (auto iter1 = distinct_values.begin(); iter1 != distinct_values.end();
       ++iter1) {
    for (auto iter2 = iter1 + 1; iter2 != distinct_values.end(); ++iter2) {
      EXPECT_NE(PolicyValueHash(*iter1), PolicyValueHash(*iter2))
          << "Index " << (iter1 - distinct_values.begin()) << " vs "
          << (iter2 - distinct_values.begin());
    }
  }
}

}  // namespace policy
