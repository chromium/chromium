// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/node_inline_data.h"

#include <string>

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

template <class T>
struct DataWithDefaultCtor : public NodeInlineData<DataWithDefaultCtor<T>> {
  T value = {};
};

template <class T>
struct DataWithExplicitCtor : public NodeInlineData<DataWithExplicitCtor<T>> {
  explicit DataWithExplicitCtor(T value) : value(std::move(value)) {}
  T value;
};

template <class T>
struct SparseDataWithDefaultCtor
    : public SparseNodeInlineData<SparseDataWithDefaultCtor<T>> {
  T value = {};
};

template <class T>
struct SparseDataWithExplicitCtor
    : public SparseNodeInlineData<SparseDataWithExplicitCtor<T>> {
  explicit SparseDataWithExplicitCtor(T value) : value(std::move(value)) {}
  T value;
};

// Tests internal::IsSparseNodeInlineData.
static_assert(!internal::IsSparseNodeInlineData<DataWithDefaultCtor<int>>);
static_assert(internal::IsSparseNodeInlineData<SparseDataWithDefaultCtor<int>>);

template <class... Args>
class DummyTestSupportsNodeInlineData : public SupportsNodeInlineData<Args...> {
 public:
  // Expose DestroyNodeInlineDataStorage().
  using SupportsNodeInlineData<Args...>::DestroyNodeInlineDataStorage;
};

TEST(NodeInlineDataTest, MustCreateStorage) {
  using TestType = DataWithDefaultCtor<int>;

  DummyTestSupportsNodeInlineData<TestType> test_supports_node_data;
  EXPECT_FALSE(TestType::Exists(&test_supports_node_data));

  TestType::Create(&test_supports_node_data);  // Does not check.
  EXPECT_TRUE(TestType::Exists(&test_supports_node_data));

  test_supports_node_data.DestroyNodeInlineDataStorage();
  EXPECT_FALSE(TestType::Exists(&test_supports_node_data));
}

template <class... Args>
class TestSupportsNodeInlineData : public SupportsNodeInlineData<Args...> {
 public:
  TestSupportsNodeInlineData() = default;
  ~TestSupportsNodeInlineData() override {
    SupportsNodeInlineData<Args...>::DestroyNodeInlineDataStorage();
  }
};

const int kTestIntValue1 = 42;
const int kTestIntValue2 = 2024;
const double kTestDoubleValue1 = 91.0;
const double kTestDoubleValue2 = 1024.0;
const char* kTestStringValue1 = "non-default value";
const char* kTestStringValue2 = "other non-default value";

TEST(NodeInlineDataDeathTest, NonSparseDefaultCtor) {
  using TestType = DataWithDefaultCtor<std::string>;

  TestSupportsNodeInlineData<TestType> test_supports_node_data;
  EXPECT_FALSE(TestType::Exists(&test_supports_node_data));
  EXPECT_CHECK_DEATH({ TestType::Get(&test_supports_node_data); });

  TestType& test_data = TestType::Create(&test_supports_node_data);
  EXPECT_TRUE(test_data.value.empty());

  test_data.value = kTestStringValue1;
  EXPECT_EQ(TestType::Get(&test_supports_node_data).value, kTestStringValue1);

  TestType::Destroy(&test_supports_node_data);
  EXPECT_FALSE(TestType::Exists(&test_supports_node_data));
}

TEST(NodeInlineDataDeathTest, NonSparseExplicitCtor) {
  using TestType = DataWithExplicitCtor<std::string>;

  TestSupportsNodeInlineData<TestType> test_supports_node_data;
  EXPECT_FALSE(TestType::Exists(&test_supports_node_data));
  EXPECT_CHECK_DEATH({ TestType::Get(&test_supports_node_data); });

  TestType& test_data =
      TestType::Create(&test_supports_node_data, kTestStringValue1);
  EXPECT_TRUE(TestType::Exists(&test_supports_node_data));
  EXPECT_EQ(test_data.value, kTestStringValue1);

  test_data.value.clear();
  EXPECT_EQ(TestType::Get(&test_supports_node_data).value, "");

  TestType::Destroy(&test_supports_node_data);
  EXPECT_FALSE(TestType::Exists(&test_supports_node_data));
}

TEST(NodeInlineDataDeathTest, SparseTypeWithDefaultCtor) {
  using TestType = SparseDataWithDefaultCtor<std::string>;

  TestSupportsNodeInlineData<TestType> test_supports_node_data;
  EXPECT_FALSE(TestType::Exists(&test_supports_node_data));
  EXPECT_CHECK_DEATH({ TestType::Get(&test_supports_node_data); });

  TestType& test_data = TestType::Create(&test_supports_node_data);
  EXPECT_TRUE(TestType::Exists(&test_supports_node_data));
  EXPECT_EQ(test_data.value, "");

  test_data.value = kTestStringValue1;
  EXPECT_EQ(TestType::Get(&test_supports_node_data).value, kTestStringValue1);

  TestType::Destroy(&test_supports_node_data);
  EXPECT_FALSE(TestType::Exists(&test_supports_node_data));
}

TEST(NodeInlineDataDeathTest, SparseTypeWithExplicitCtor) {
  using TestType = SparseDataWithExplicitCtor<std::string>;

  TestSupportsNodeInlineData<TestType> test_supports_node_data;
  EXPECT_FALSE(TestType::Exists(&test_supports_node_data));
  EXPECT_CHECK_DEATH({ TestType::Get(&test_supports_node_data); });

  TestType& test_data =
      TestType::Create(&test_supports_node_data, kTestStringValue1);
  EXPECT_TRUE(TestType::Exists(&test_supports_node_data));
  EXPECT_EQ(test_data.value, kTestStringValue1);

  test_data.value.clear();
  EXPECT_EQ(TestType::Get(&test_supports_node_data).value, "");

  TestType::Destroy(&test_supports_node_data);
  EXPECT_FALSE(TestType::Exists(&test_supports_node_data));
}

TEST(NodeInlineDataTest, MixMultipleTypesSameStorage) {
  using TestTypeInt = DataWithExplicitCtor<int>;
  using TestTypeDouble = DataWithExplicitCtor<double>;
  using TestTypeString = DataWithExplicitCtor<std::string>;

  TestSupportsNodeInlineData<TestTypeInt, TestTypeDouble, TestTypeString>
      test_supports_node_data;
  EXPECT_FALSE(TestTypeInt::Exists(&test_supports_node_data));
  EXPECT_FALSE(TestTypeDouble::Exists(&test_supports_node_data));
  EXPECT_FALSE(TestTypeString::Exists(&test_supports_node_data));

  TestTypeInt& test_type_int =
      TestTypeInt::Create(&test_supports_node_data, kTestIntValue1);
  EXPECT_TRUE(TestTypeInt::Exists(&test_supports_node_data));
  EXPECT_FALSE(TestTypeDouble::Exists(&test_supports_node_data));
  EXPECT_FALSE(TestTypeString::Exists(&test_supports_node_data));

  TestTypeDouble& test_type_double =
      TestTypeDouble::Create(&test_supports_node_data, kTestDoubleValue1);
  EXPECT_TRUE(TestTypeInt::Exists(&test_supports_node_data));
  EXPECT_TRUE(TestTypeDouble::Exists(&test_supports_node_data));
  EXPECT_FALSE(TestTypeString::Exists(&test_supports_node_data));

  TestTypeString& test_type_string =
      TestTypeString::Create(&test_supports_node_data, kTestStringValue1);
  EXPECT_TRUE(TestTypeInt::Exists(&test_supports_node_data));
  EXPECT_TRUE(TestTypeDouble::Exists(&test_supports_node_data));
  EXPECT_TRUE(TestTypeString::Exists(&test_supports_node_data));
  EXPECT_EQ(test_type_int.value, kTestIntValue1);
  EXPECT_EQ(test_type_double.value, kTestDoubleValue1);
  EXPECT_EQ(test_type_string.value, kTestStringValue1);

  test_type_int.value = kTestIntValue2;
  test_type_double.value = kTestDoubleValue2;
  test_type_string.value = kTestStringValue2;
  EXPECT_EQ(TestTypeInt::Get(&test_supports_node_data).value, kTestIntValue2);
  EXPECT_EQ(TestTypeDouble::Get(&test_supports_node_data).value,
            kTestDoubleValue2);
  EXPECT_EQ(TestTypeString::Get(&test_supports_node_data).value,
            kTestStringValue2);

  TestTypeInt::Destroy(&test_supports_node_data);
  EXPECT_FALSE(TestTypeInt::Exists(&test_supports_node_data));
  EXPECT_TRUE(TestTypeDouble::Exists(&test_supports_node_data));
  EXPECT_TRUE(TestTypeString::Exists(&test_supports_node_data));

  TestTypeDouble::Destroy(&test_supports_node_data);
  EXPECT_FALSE(TestTypeInt::Exists(&test_supports_node_data));
  EXPECT_FALSE(TestTypeDouble::Exists(&test_supports_node_data));
  EXPECT_TRUE(TestTypeString::Exists(&test_supports_node_data));

  TestTypeString::Destroy(&test_supports_node_data);
  EXPECT_FALSE(TestTypeInt::Exists(&test_supports_node_data));
  EXPECT_FALSE(TestTypeDouble::Exists(&test_supports_node_data));
  EXPECT_FALSE(TestTypeString::Exists(&test_supports_node_data));
}

TEST(NodeInlineDataTest, MixMultipleTypesDifferentStorage) {
  using NonSparseTestType = DataWithExplicitCtor<int>;
  using SparseTestType = SparseDataWithExplicitCtor<double>;

  TestSupportsNodeInlineData<NonSparseTestType, SparseTestType>
      test_supports_node_data;
  EXPECT_FALSE(NonSparseTestType::Exists(&test_supports_node_data));
  EXPECT_FALSE(SparseTestType::Exists(&test_supports_node_data));

  NonSparseTestType& non_sparse_test_type =
      NonSparseTestType::Create(&test_supports_node_data, kTestIntValue1);
  SparseTestType& sparse_test_type =
      SparseTestType::Create(&test_supports_node_data, kTestDoubleValue1);
  EXPECT_TRUE(NonSparseTestType::Exists(&test_supports_node_data));
  EXPECT_TRUE(SparseTestType::Exists(&test_supports_node_data));
  EXPECT_EQ(non_sparse_test_type.value, kTestIntValue1);
  EXPECT_EQ(sparse_test_type.value, kTestDoubleValue1);

  non_sparse_test_type.value = kTestIntValue2;
  sparse_test_type.value = kTestDoubleValue2;
  EXPECT_EQ(NonSparseTestType::Get(&test_supports_node_data).value,
            kTestIntValue2);
  EXPECT_EQ(SparseTestType::Get(&test_supports_node_data).value,
            kTestDoubleValue2);

  SparseTestType::Destroy(&test_supports_node_data);
  EXPECT_TRUE(NonSparseTestType::Exists(&test_supports_node_data));
  EXPECT_FALSE(SparseTestType::Exists(&test_supports_node_data));
}

TEST(NodeInlineDataTest, ConstAccessors) {
  using NonSparseTestType = DataWithExplicitCtor<std::string>;
  using SparseTestType = SparseDataWithExplicitCtor<std::string>;

  TestSupportsNodeInlineData<NonSparseTestType, SparseTestType>
      test_supports_node_data;
  NonSparseTestType::Create(&test_supports_node_data, kTestStringValue1);
  SparseTestType::Create(&test_supports_node_data, kTestStringValue2);

  // Make sure getters compile when passed a const pointer.
  const auto* const_ptr = &test_supports_node_data;
  const auto& non_sparse_test_type = NonSparseTestType::Get(const_ptr);
  const auto& sparse_test_type = SparseTestType::Get(const_ptr);
  EXPECT_EQ(non_sparse_test_type.value, kTestStringValue1);
  EXPECT_EQ(sparse_test_type.value, kTestStringValue2);
}

}  // namespace performance_manager
