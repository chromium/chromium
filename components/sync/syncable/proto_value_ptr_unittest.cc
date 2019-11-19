// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/syncable/proto_value_ptr.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

// Simple test struct that wraps an integer
struct IntValue {
 public:
  explicit IntValue(int value) { value_ = value; }
  int value() { return value_; }

 private:
  int value_;
};

// TestValue class is used as a template argument with ProtoValuePtr<T>
class TestValue {
 public:
  TestValue() : value_(nullptr), is_default_(false) {}
  explicit TestValue(int value)
      : value_(new IntValue(value)), is_default_(false) {}

  ~TestValue() { g_delete_count++; }

  static void ResetCounters() {
    g_copy_count = 0;
    g_parse_count = 0;
    g_delete_count = 0;
  }

  static int copy_count() { return g_copy_count; }
  static int parse_count() { return g_parse_count; }
  static int delete_count() { return g_delete_count; }

  int value() const { return value_->value(); }
  IntValue* value_ptr() const { return value_.get(); }
  bool is_initialized() const { return !!value_; }
  bool is_default() const { return is_default_; }

  // TestValue uses the default traits struct with ProtoValuePtr<TestValue>.
  // The following 4 functions are expected by the traits struct to exist
  // in this class.
  void CopyFrom(const TestValue& from) {
    // Expected to always copy from an initialized instance
    // to an uninitialized one.
    // Not expected either value to be default.
    ASSERT_FALSE(is_initialized());
    ASSERT_FALSE(is_default());
    ASSERT_TRUE(from.is_initialized());
    ASSERT_FALSE(from.is_default());
    value_ = std::make_unique<IntValue>(from.value());
    g_copy_count++;
  }

  void Swap(TestValue* src) {
    // Expected to always swap with an initialized instance.
    // The current instance must always be an uninitialized one.
    // Not expected either value to be default.
    ASSERT_FALSE(is_initialized());
    ASSERT_FALSE(is_default());
    ASSERT_TRUE(src->is_initialized());
    ASSERT_FALSE(src->is_default());
    // Not exactly swap, but good enough for the test.
    value_ = std::move(src->value_);
  }

  void ParseFromArray(const void* blob, int length) {
    // Similarly to CopyFrom this is expected to be called on
    // an uninitialized instance.
    ASSERT_FALSE(is_initialized());
    ASSERT_FALSE(is_default());
    // The blob is an address of an integer
    ASSERT_EQ(static_cast<int>(sizeof(int)), length);
    value_ = std::make_unique<IntValue>(*static_cast<const int*>(blob));
    g_parse_count++;
  }

  int ByteSize() const { return is_initialized() ? sizeof(int) : 0; }

  static const TestValue& default_instance() {
    static TestValue default_instance;
    default_instance.is_default_ = true;
    return default_instance;
  }

 private:
  static int g_copy_count;
  static int g_parse_count;
  static int g_delete_count;

  std::unique_ptr<IntValue> value_;
  bool is_default_;

  DISALLOW_COPY_AND_ASSIGN(TestValue);
};

// Static initializers.
int TestValue::g_copy_count = 0;
int TestValue::g_parse_count = 0;
int TestValue::g_delete_count = 0;

}  // namespace

using TestPtr = ProtoValuePtr<TestValue>;

class ProtoValuePtrTest : public testing::Test {
 public:
  void SetUp() override { TestValue::ResetCounters(); }

  static bool WrappedValuesAreShared(const TestPtr& ptr1, const TestPtr& ptr2) {
    const TestValue& wrapped_value_1 = ptr1.value();
    const TestValue& wrapped_value_2 = ptr2.value();
    // Compare addresses.
    return &wrapped_value_1 == &wrapped_value_2;
  }
};

TEST_F(ProtoValuePtrTest, ValueAssignment) {
  // Basic assignment and default value.
  TestValue t1(1);
  {
    TestPtr ptr1;
    EXPECT_TRUE(ptr1->is_default());

    ptr1.set_value(t1);
    EXPECT_FALSE(ptr1->is_default());
    EXPECT_EQ(1, ptr1->value());
  }

  EXPECT_EQ(1, TestValue::copy_count());
  EXPECT_EQ(1, TestValue::delete_count());
}

TEST_F(ProtoValuePtrTest, ValueSwap) {
  TestValue t2(2);
  {
    TestPtr ptr2;
    EXPECT_TRUE(ptr2->is_default());

    IntValue* inner_ptr = t2.value_ptr();

    ptr2.swap_value(&t2);
    EXPECT_FALSE(ptr2->is_default());
    EXPECT_EQ(2, ptr2->value());
    EXPECT_EQ(inner_ptr, ptr2->value_ptr());
  }

  EXPECT_EQ(0, TestValue::copy_count());
  EXPECT_EQ(1, TestValue::delete_count());
}

TEST_F(ProtoValuePtrTest, SharingTest) {
  // Sharing between two pointers.
  TestValue empty;
  TestValue t2(2);
  TestValue t3(3);
  {
    TestPtr ptr2;
    TestPtr ptr3;

    EXPECT_TRUE(ptr2->is_default());
    EXPECT_TRUE(ptr3->is_default());
    EXPECT_EQ(0, TestValue::copy_count());
    EXPECT_EQ(0, TestValue::delete_count());

    ptr2.set_value(t2);
    EXPECT_EQ(1, TestValue::copy_count());
    EXPECT_EQ(0, TestValue::delete_count());

    ptr3 = ptr2;
    // Both |ptr2| and |ptr3| now share the same value "2".
    // No additional copies expected.
    EXPECT_EQ(1, TestValue::copy_count());
    EXPECT_EQ(0, TestValue::delete_count());
    EXPECT_FALSE(ptr3->is_default());
    EXPECT_EQ(2, ptr3->value());
    EXPECT_TRUE(WrappedValuesAreShared(ptr2, ptr3));

    // Stop sharing - |ptr2| is "3" and |ptr3| is still "2".
    ptr2.set_value(t3);
    EXPECT_FALSE(WrappedValuesAreShared(ptr2, ptr3));
    EXPECT_EQ(3, ptr2->value());
    EXPECT_EQ(2, ptr3->value());
    // No extra copies or deletions expected.
    EXPECT_EQ(2, TestValue::copy_count());
    EXPECT_EQ(0, TestValue::delete_count());

    // |ptr3| still has the old value.
    EXPECT_EQ(2, ptr3->value());

    // Share again. Both values are "3".
    ptr3 = ptr2;
    EXPECT_EQ(3, ptr3->value());
    // This should have resulted in deleting the wrapper for the value "2".
    EXPECT_EQ(1, TestValue::delete_count());
    // No extra copies expected.
    EXPECT_EQ(2, TestValue::copy_count());

    // Set default value to one of the pointers.
    ptr2.set_value(empty);
    EXPECT_TRUE(ptr2->is_default());
    // The other one is still intact.
    EXPECT_FALSE(ptr3->is_default());
    EXPECT_EQ(3, ptr3->value());
    // No extra copies or deletions expected.
    EXPECT_EQ(1, TestValue::delete_count());
    EXPECT_EQ(2, TestValue::copy_count());

    // Copy the default value between the pointers.
    ptr3 = ptr2;
    EXPECT_TRUE(ptr3->is_default());
    // The wrapper for "3" is now deleted.
    EXPECT_EQ(2, TestValue::delete_count());
  }

  // No extra deletions expected upon leaving the scope.
  EXPECT_EQ(2, TestValue::delete_count());
}

TEST_F(ProtoValuePtrTest, ParsingTest) {
  int v1 = 21;

  {
    TestPtr ptr1;

    ptr1.load(&v1, sizeof(int));

    EXPECT_EQ(1, TestValue::parse_count());
    EXPECT_EQ(0, TestValue::copy_count());

    EXPECT_EQ(v1, ptr1->value());
  }

  EXPECT_EQ(1, TestValue::delete_count());
}

}  // namespace syncer
