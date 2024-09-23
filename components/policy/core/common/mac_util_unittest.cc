// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/mac_util.h"

#include <CoreFoundation/CoreFoundation.h>

#include <memory>

#include "base/apple/scoped_cftyperef.h"
#include "base/values.h"
#include "components/policy/core/common/policy_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

// Test checks that base::Value converted to CFPropertyList with
// ValueToProperty() is successfully restored from the property with
// PropertyToValue().
TEST(PolicyMacUtilTest, ValueToPropertyRoundTrip) {
  base::Value::Dict root;

  // base::Value::Type::NONE
  root.Set("null", base::Value());

  // base::Value::Type::BOOLEAN
  root.Set("false", false);
  root.Set("true", true);

  // base::Value::Type::INTEGER
  root.Set("int", 123);
  root.Set("zero", 0);

  // base::Value::Type::DOUBLE
  root.Set("double", 123.456);
  root.Set("zerod", 0.0);

  // base::Value::Type::STRING
  root.Set("string", "the fox jumps over something");
  root.Set("empty", "");

  // base::Value::Type::LIST
  root.Set("emptyl", base::Value::List());
  base::Value::List list;
  for (const auto [key, value] : root) {
    list.Append(value.Clone());
  }
  EXPECT_EQ(root.size(), list.size());
  list.Append(root.Clone());
  root.Set("list", list.Clone());

  // base::Value::Type::DICT
  root.Set("emptyd", base::Value::Dict());

  // Key with dots.
  root.Set("key.with.dots", 789);

  // Very meta.
  root.Set("dict", root.Clone());

  const base::Value root_val(std::move(root));
  // base::Value -> property list -> base::Value.
  base::apple::ScopedCFTypeRef<CFPropertyListRef> property =
      ValueToProperty(root_val);
  ASSERT_TRUE(property);
  std::unique_ptr<base::Value> value = PropertyToValue(property.get());
  ASSERT_TRUE(value);
  EXPECT_EQ(root_val, *value);
}

}  // namespace policy
