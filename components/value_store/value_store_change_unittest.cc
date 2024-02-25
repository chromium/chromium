// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/value_store/value_store_change.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Value;

namespace value_store {

TEST(ValueStoreChangeTest, ToValue) {
  ValueStoreChangeList changes;
  changes.push_back(ValueStoreChange("foo", std::nullopt, base::Value("bar")));
  changes.push_back(ValueStoreChange("baz", base::Value("qux"), std::nullopt));

  base::Value::Dict expected;
  base::Value::Dict expected_foo;
  base::Value::Dict expected_baz;
  expected_foo.Set("newValue", "bar");
  expected_baz.Set("oldValue", "qux");
  expected.Set("foo", std::move(expected_foo));
  expected.Set("baz", std::move(expected_baz));

  EXPECT_EQ(expected, ValueStoreChange::ToValue(std::move(changes)));
}

}  // namespace value_store
