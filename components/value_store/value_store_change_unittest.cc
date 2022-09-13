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
  changes.push_back(ValueStoreChange("foo", absl::nullopt, base::Value("bar")));
  changes.push_back(ValueStoreChange("baz", base::Value("qux"), absl::nullopt));

  base::Value expected(base::Value::Type::DICTIONARY);
  base::Value expected_foo(base::Value::Type::DICTIONARY);
  base::Value expected_baz(base::Value::Type::DICTIONARY);
  expected_foo.SetStringKey("newValue", "bar");
  expected_baz.SetStringKey("oldValue", "qux");
  expected.SetKey("foo", std::move(expected_foo));
  expected.SetKey("baz", std::move(expected_baz));

  EXPECT_EQ(expected, ValueStoreChange::ToValue(std::move(changes)));
}

}  // namespace value_store
