// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tabs/public/supports_handles.h"

#include <concepts>
#include <cstdint>
#include <memory>

#include "testing/gtest/include/gtest/gtest-death-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs {

namespace {
class TestClass : public SupportsHandles<TestClass> {};
}  // namespace

class SupportsHandlesTest : public testing::Test {
 public:
  template <typename T>
  void SetCounter(int32_t value) {
    auto& helper = internal::HandleHelper<T>::GetInstance();
    DCHECK_CALLED_ON_VALID_SEQUENCE(helper.sequence_);
    CHECK(helper.lookup_table_.empty());
    helper.last_handle_value_ = value;
  }
};

TEST_F(SupportsHandlesTest, NullHandle) {
  EXPECT_EQ(nullptr, TestClass::Handle::Null().Get());
}

TEST_F(SupportsHandlesTest, ValidHandle) {
  TestClass c;
  EXPECT_EQ(&c, c.GetHandle().Get());
}

TEST_F(SupportsHandlesTest, HandleBecomesInvalid) {
  TestClass::Handle handle;
  {
    TestClass c;
    handle = c.GetHandle();
  }
  EXPECT_EQ(nullptr, handle.Get());
}

TEST_F(SupportsHandlesTest, IncrementsValues) {
  SetCounter<TestClass>(TestClass::Handle::NullValue);
  TestClass c1;
  TestClass c2;
  TestClass c3;
  EXPECT_EQ(1, c1.GetHandle().raw_value());
  EXPECT_EQ(2, c2.GetHandle().raw_value());
  EXPECT_EQ(3, c3.GetHandle().raw_value());
}

TEST_F(SupportsHandlesTest, FailsOnRolloverSigned) {
  SetCounter<TestClass>(TestClass::Handle::NullValue - 1);
  EXPECT_DEATH_IF_SUPPORTED(TestClass c, "");
}

}  // namespace tabs
