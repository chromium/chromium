// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/mac_util.h"

#include <CoreFoundation/CoreFoundation.h>

#include <memory>

#include "base/mac/scoped_cftyperef.h"
#include "base/values.h"
#include "components/policy/core/common/policy_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

// Test checks that base::Value converted to CFPropertyList with
// ValueToProperty() is successfully restored from the property with
// PropertyToValue().
TEST(PolicyMacUtilTest, ValueToPropertyRoundTrip) {
  base::DictionaryValue root;

  // base::Value::Type::NONE
  root.Set("null", std::make_unique<base::Value>());

  // base::Value::Type::BOOLEAN
  root.SetBoolKey("false", false);
  root.SetBoolKey("true", true);

  // base::Value::Type::INTEGER
  root.SetIntKey("int", 123);
  root.SetIntKey("zero", 0);

  // base::Value::Type::DOUBLE
  root.SetDoubleKey("double", 123.456);
  root.SetDoubleKey("zerod", 0.0);

  // base::Value::Type::STRING
  root.SetStringKey("string", "the fox jumps over something");
  root.SetStringKey("empty", "");

  // base::Value::Type::LIST
  root.Set("emptyl", std::make_unique<base::Value>(base::Value::Type::LIST));
  base::ListValue list;
  for (const auto item : root.GetDict())
    list.GetList().Append(item.second.Clone());
  EXPECT_EQ(root.DictSize(), list.GetList().size());
  list.GetList().Append(root.Clone());
  root.SetKey("list", list.Clone());

  // base::Value::Type::DICTIONARY
  root.Set("emptyd",
           std::make_unique<base::Value>(base::Value::Type::DICTIONARY));

  // Key with dots.
  root.SetIntKey("key.with.dots", 789);

  // Very meta.
  root.SetKey("dict", root.Clone());

  // base::Value -> property list -> base::Value.
  base::ScopedCFTypeRef<CFPropertyListRef> property(ValueToProperty(root));
  ASSERT_TRUE(property);
  std::unique_ptr<base::Value> value = PropertyToValue(property);
  ASSERT_TRUE(value);
  EXPECT_EQ(root, *value);
}

}  // namespace policy
