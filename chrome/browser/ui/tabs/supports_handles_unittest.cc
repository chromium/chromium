// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/supports_handles.h"

#include <concepts>
#include <cstdint>
#include <memory>

#include "testing/gtest/include/gtest/gtest-death-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class DefaultTestClass : public SupportsHandles<DefaultTestClass> {};
template <std::integral V>
class TestClass : public SupportsHandles<TestClass<V>, V> {};
}  // namespace

class SupportsHandlesTest : public testing::Test {
 public:
  template <typename T>
  void ResetCounter() {
    auto& helper =
        internal::HandleHelper<T,
                               typename T::Handle::RawValueType>::GetInstance();
    DCHECK_CALLED_ON_VALID_SEQUENCE(helper.sequence_);
    CHECK(helper.lookup_table_.empty());
    helper.last_handle_value_ = 0;
  }

 private:
};

TEST_F(SupportsHandlesTest, NullHandle) {
  using C = TestClass<int32_t>;
  EXPECT_EQ(nullptr, C::Handle::Null().Get());
}

TEST_F(SupportsHandlesTest, ValidHandle) {
  using C = TestClass<int32_t>;
  C c;
  EXPECT_EQ(&c, c.GetHandle().Get());
}

TEST_F(SupportsHandlesTest, HandleBecomesInvalid) {
  using C = TestClass<int32_t>;
  C::Handle handle;
  {
    C c;
    handle = c.GetHandle();
  }
  EXPECT_EQ(nullptr, handle.Get());
}

// Verifies that ubsan CI builds can handle default template parameter.
TEST_F(SupportsHandlesTest, DefaultHandleBecomesInvalid) {
  DefaultTestClass::Handle handle;
  {
    DefaultTestClass c;
    handle = c.GetHandle();
  }
  EXPECT_EQ(nullptr, handle.Get());
}

TEST_F(SupportsHandlesTest, IncrementsValues) {
  using C = TestClass<int32_t>;
  ResetCounter<C>();
  C c1;
  C c2;
  C c3;
  EXPECT_EQ(1, c1.GetHandle().raw_value());
  EXPECT_EQ(2, c2.GetHandle().raw_value());
  EXPECT_EQ(3, c3.GetHandle().raw_value());
}

TEST_F(SupportsHandlesTest, FailsOnRolloverUnsigned) {
  using C = TestClass<uint8_t>;
  ResetCounter<C>();
  for (int i = 1; i < 256; ++i) {
    C c;
  }
  EXPECT_DEATH_IF_SUPPORTED(C c, "");
}

TEST_F(SupportsHandlesTest, FailsOnRolloverSigned) {
  using C = TestClass<int8_t>;
  ResetCounter<C>();
  for (int i = 1; i < 256; ++i) {
    C c;
  }
  EXPECT_DEATH_IF_SUPPORTED(C c, "");
}
