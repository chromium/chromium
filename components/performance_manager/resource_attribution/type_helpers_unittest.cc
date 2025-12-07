// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/type_helpers.h"

#include <optional>
#include <type_traits>
#include <variant>
#include <vector>

#include "base/types/optional_ref.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_attribution::internal {

namespace {

template <typename T>
::testing::AssertionResult TestOptionalConstRef(base::optional_ref<T> opt_ref,
                                                auto expected_value) {
  static_assert(std::is_const_v<T>, "Expected optional_ref<const T>");
  if (!opt_ref.has_value()) {
    return ::testing::AssertionFailure() << "optional_ref contains nullopt";
  }
  if (opt_ref.value() != expected_value) {
    return ::testing::AssertionFailure()
           << "optional_ref had wrong value: expected " << expected_value
           << ", got " << opt_ref.value();
  }
  return ::testing::AssertionSuccess();
}

TEST(ResourceAttrTypeHelpersTest, IsVariantAlternativeEmptyVariant) {
  static_assert(!kIsVariantAlternative<int, std::variant<>>);
}

TEST(ResourceAttrTypeHelpersTest, IsVariantAlternativeSingleAlternative) {
  using V = std::variant<int>;
  static_assert(kIsVariantAlternative<int, V>);
  static_assert(!kIsVariantAlternative<double, V>);
}

TEST(ResourceAttrTypeHelpersTest, IsVariantAlternativeManyAlternatives) {
  using V = std::variant<int, double>;
  static_assert(kIsVariantAlternative<int, V>);
  static_assert(kIsVariantAlternative<double, V>);
  static_assert(!kIsVariantAlternative<bool, V>);
}

TEST(ResourceAttrTypeHelpersTest, IsVariantAlternativeWithMonostate) {
  using V = std::variant<std::monostate, int>;
  static_assert(kIsVariantAlternative<int, V>);
  static_assert(!kIsVariantAlternative<double, V>);
}

// Can't test GetAsOptional() with std::variant<> because it can't be
// instantiated.

TEST(ResourceAttrTypeHelpersTest, GetAsOptionalSingleAlternative) {
  std::variant<int> v;
  EXPECT_EQ(GetAsOptional<int>(v), 0);
  v = 1;
  EXPECT_EQ(GetAsOptional<int>(v), 1);
}

TEST(ResourceAttrTypeHelpersTest, GetAsOptionalManyAlternatives) {
  std::variant<int, double> v;
  EXPECT_EQ(GetAsOptional<int>(v), 0);
  EXPECT_EQ(GetAsOptional<double>(v), std::nullopt);
  v = 1;
  EXPECT_EQ(GetAsOptional<int>(v), 1);
  EXPECT_EQ(GetAsOptional<double>(v), std::nullopt);
  v = 2.0;
  EXPECT_EQ(GetAsOptional<int>(v), std::nullopt);
  EXPECT_EQ(GetAsOptional<double>(v), 2.0);
}

TEST(ResourceAttrTypeHelpersTest, GetAsOptionalWithMonostate) {
  std::variant<std::monostate, int> v;
  EXPECT_EQ(GetAsOptional<int>(v), std::nullopt);
  v = 1;
  EXPECT_EQ(GetAsOptional<int>(v), 1);
}

// Note: GetFromVariantVector returns `base::optional_ref`, which doesn't define
// all comparators, so need to match against the contained values.

TEST(ResourceAttrTypeHelpersTest, VariantVectorSingleAlternative) {
  using TestVariant = std::variant<int>;
  std::vector<TestVariant> vs;
  EXPECT_FALSE(VariantVectorContains<int>(vs));
  EXPECT_FALSE(GetFromVariantVector<int>(vs).has_value());
  vs.push_back(1);
  EXPECT_TRUE(VariantVectorContains<int>(vs));
  ASSERT_TRUE(GetFromVariantVector<int>(vs).has_value());
  EXPECT_EQ(GetFromVariantVector<int>(vs).value(), 1);
  // First matching element should be returned.
  vs.push_back(2);
  EXPECT_TRUE(VariantVectorContains<int>(vs));
  ASSERT_TRUE(GetFromVariantVector<int>(vs).has_value());
  EXPECT_EQ(GetFromVariantVector<int>(vs).value(), 1);
}

TEST(ResourceAttrTypeHelpersTest, VariantVectorManyAlternatives) {
  using TestVariant = std::variant<int, double>;
  std::vector<TestVariant> vs;
  EXPECT_FALSE(VariantVectorContains<int>(vs));
  EXPECT_FALSE(GetFromVariantVector<int>(vs).has_value());
  EXPECT_FALSE(VariantVectorContains<double>(vs));
  EXPECT_FALSE(GetFromVariantVector<double>(vs).has_value());
  vs.push_back(1);
  EXPECT_TRUE(VariantVectorContains<int>(vs));
  ASSERT_TRUE(GetFromVariantVector<int>(vs).has_value());
  EXPECT_EQ(GetFromVariantVector<int>(vs).value(), 1);
  EXPECT_FALSE(VariantVectorContains<double>(vs));
  EXPECT_FALSE(GetFromVariantVector<double>(vs).has_value());
  vs.push_back(2.0);
  EXPECT_TRUE(VariantVectorContains<int>(vs));
  ASSERT_TRUE(GetFromVariantVector<int>(vs).has_value());
  EXPECT_EQ(GetFromVariantVector<int>(vs).value(), 1);
  EXPECT_TRUE(VariantVectorContains<double>(vs));
  ASSERT_TRUE(GetFromVariantVector<double>(vs).has_value());
  EXPECT_EQ(GetFromVariantVector<double>(vs).value(), 2.0);
  vs.erase(vs.begin());
  EXPECT_FALSE(VariantVectorContains<int>(vs));
  EXPECT_FALSE(GetFromVariantVector<int>(vs).has_value());
  EXPECT_TRUE(VariantVectorContains<double>(vs));
  ASSERT_TRUE(GetFromVariantVector<double>(vs).has_value());
  EXPECT_EQ(GetFromVariantVector<double>(vs).value(), 2.0);
}

TEST(ResourceAttrTypeHelpersTest, VariantVectorWithMonostate) {
  using TestVariant = std::variant<std::monostate, int>;
  std::vector<TestVariant> vs;
  EXPECT_FALSE(VariantVectorContains<int>(vs));
  EXPECT_FALSE(GetFromVariantVector<int>(vs).has_value());
  vs.push_back(TestVariant{});
  EXPECT_FALSE(VariantVectorContains<int>(vs));
  EXPECT_FALSE(GetFromVariantVector<int>(vs).has_value());
  vs.push_back(1);
  EXPECT_TRUE(VariantVectorContains<int>(vs));
  ASSERT_TRUE(GetFromVariantVector<int>(vs).has_value());
  EXPECT_EQ(GetFromVariantVector<int>(vs).value(), 1);
}

TEST(ResourceAttrTypeHelpersTest, VariantVectorConst) {
  using TestVariant = std::variant<const int, double>;
  std::vector<TestVariant> mutable_vec{1, 2.3};
  const std::vector<TestVariant> const_vec = {1, 2.3};

  // Can never mutate the `const int` element. (GetFromVariantVector<int>(...)
  // fails to compile.)
  EXPECT_TRUE(VariantVectorContains<const int>(mutable_vec));
  EXPECT_TRUE(
      TestOptionalConstRef(GetFromVariantVector<const int>(mutable_vec), 1));
  EXPECT_TRUE(
      TestOptionalConstRef(GetFromVariantVector<const int>(const_vec), 1));

  // Can only mutate the `double` element in `mutable_vec`.
  // GetFromVariantVector<double>(...) returns optional_ref<const double> in
  // others.
  EXPECT_TRUE(VariantVectorContains<double>(mutable_vec));
  ASSERT_TRUE(GetFromVariantVector<double>(mutable_vec).has_value());
  GetFromVariantVector<double>(mutable_vec).value() = 4.5;
  EXPECT_EQ(GetFromVariantVector<double>(mutable_vec).value(), 4.5);
  EXPECT_TRUE(TestOptionalConstRef(
      GetFromVariantVector<const double>(mutable_vec), 4.5));

  EXPECT_TRUE(VariantVectorContains<double>(const_vec));
  EXPECT_TRUE(
      TestOptionalConstRef(GetFromVariantVector<double>(const_vec), 2.3));
  EXPECT_TRUE(VariantVectorContains<const double>(const_vec));
  EXPECT_TRUE(
      TestOptionalConstRef(GetFromVariantVector<const double>(const_vec), 2.3));
}

// Can't test comparators with std::variant<> because it can't be
// instantiated.

TEST(ResourceAttrTypeHelpersTest, VariantComparatorsSingleAlternative) {
  using TestVariant = std::variant<int>;
  TestVariant v = 1;

  // Make sure the overloaded comparators don't interfere with the default
  // variant comparators.
  EXPECT_EQ(v, std::variant<int>(1));
  EXPECT_NE(v, std::variant<int>(-1));
  EXPECT_LT(v, std::variant<int>(2));
  EXPECT_LE(v, std::variant<int>(2));
  EXPECT_LE(v, std::variant<int>(1));
  EXPECT_GT(v, std::variant<int>(0));
  EXPECT_GE(v, std::variant<int>(0));
  EXPECT_GE(v, std::variant<int>(1));

  // Only == and != are defined for variants and their alternative types, and
  // only for variants in the resource_attriution namespace. The rich EXPECT_EQ
  // macros can't be used since they're not in the same namespce.
  EXPECT_TRUE(v == 1);
  EXPECT_FALSE(v == 0);
  EXPECT_TRUE(v != -1);
  EXPECT_FALSE(v != 1);
}

TEST(ResourceAttrTypeHelpersTest, VariantComparatorsManyAlternatives) {
  using TestVariant = std::variant<int, double>;
  TestVariant v_int = 1;
  TestVariant v_double = 1.0;

  // Default comparators.
  EXPECT_EQ(v_int, TestVariant(1));
  EXPECT_EQ(v_double, TestVariant(1.0));
  EXPECT_NE(v_int, v_double);

  // Overloaded comparators.
  EXPECT_TRUE(v_int == 1);
  EXPECT_FALSE(v_int == 0);
  EXPECT_FALSE(v_int == 1.0);
  EXPECT_TRUE(v_int != -1);
  EXPECT_FALSE(v_int != 1);
  EXPECT_TRUE(v_int != 1.0);

  EXPECT_TRUE(v_double == 1.0);
  EXPECT_FALSE(v_double == 0.0);
  EXPECT_FALSE(v_double == 1);
  EXPECT_TRUE(v_double != -1.0);
  EXPECT_FALSE(v_double != 1.0);
  EXPECT_TRUE(v_double != 1);
}

}  // namespace

}  // namespace resource_attribution::internal
