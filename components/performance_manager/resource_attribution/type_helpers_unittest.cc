// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/type_helpers.h"

#include <type_traits>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace performance_manager::resource_attribution::internal {

namespace {

// gtest EXPECT macros interpret spaces as parameter breaks, so won't compile
// with parameters like "IsVariantAlternative<T, V>". This class wraps functions
// in type_helpers.h in methods that take only a single template param.
template <typename V>
class VariantTester {
 public:
  template <typename T>
  static bool IsVariantAlternativeValue() {
    return IsVariantAlternative<T, V>::value;
  }

  // Template resolution will match this version of the function if T is an
  // alternative of the variant V.
  template <typename T, EnableIfIsVariantAlternative<T, V> = true>
  static bool ConditionallyEnabled() {
    return true;
  }

  // Template resolution will match this version of the function if T is NOT an
  // alternative of the variant V.
  template <typename T,
            std::enable_if_t<!IsVariantAlternative<T, V>::value, bool> = true>
  static bool ConditionallyEnabled() {
    return false;
  }
};

TEST(ResourceAttributionTypeHelpersTest, IsVariantAlternativeEmptyVariant) {
  using Tester = VariantTester<absl::variant<>>;
  EXPECT_FALSE(Tester::IsVariantAlternativeValue<int>());
  EXPECT_FALSE(Tester::ConditionallyEnabled<int>());
}

TEST(ResourceAttributionTypeHelpersTest,
     IsVariantAlternativeSingleAlternative) {
  using Tester = VariantTester<absl::variant<int>>;
  EXPECT_TRUE(Tester::IsVariantAlternativeValue<int>());
  EXPECT_TRUE(Tester::ConditionallyEnabled<int>());
  EXPECT_FALSE(Tester::IsVariantAlternativeValue<double>());
  EXPECT_FALSE(Tester::ConditionallyEnabled<double>());
}

TEST(ResourceAttributionTypeHelpersTest, IsVariantAlternativeManyAlternatives) {
  using Tester = VariantTester<absl::variant<int, double>>;
  EXPECT_TRUE(Tester::IsVariantAlternativeValue<int>());
  EXPECT_TRUE(Tester::ConditionallyEnabled<int>());
  EXPECT_TRUE(Tester::IsVariantAlternativeValue<double>());
  EXPECT_TRUE(Tester::ConditionallyEnabled<double>());
  EXPECT_FALSE(Tester::IsVariantAlternativeValue<bool>());
  EXPECT_FALSE(Tester::ConditionallyEnabled<bool>());
}

TEST(ResourceAttributionTypeHelpersTest, IsVariantAlternativeWithMonostate) {
  using Tester = VariantTester<absl::variant<absl::monostate, int>>;
  EXPECT_TRUE(Tester::IsVariantAlternativeValue<int>());
  EXPECT_TRUE(Tester::ConditionallyEnabled<int>());
  EXPECT_FALSE(Tester::IsVariantAlternativeValue<double>());
  EXPECT_FALSE(Tester::ConditionallyEnabled<double>());
}

// Can't test GetAsOptional() with absl::variant<> because it can't be
// instantiated.

TEST(ResourceAttributionTypeHelpersTest, GetAsOptionalSingleAlternative) {
  absl::variant<int> v;
  EXPECT_EQ(GetAsOptional<int>(v), 0);
  v = 1;
  EXPECT_EQ(GetAsOptional<int>(v), 1);
}

TEST(ResourceAttributionTypeHelpersTest, GetAsOptionalManyAlternatives) {
  absl::variant<int, double> v;
  EXPECT_EQ(GetAsOptional<int>(v), 0);
  EXPECT_EQ(GetAsOptional<double>(v), absl::nullopt);
  v = 1;
  EXPECT_EQ(GetAsOptional<int>(v), 1);
  EXPECT_EQ(GetAsOptional<double>(v), absl::nullopt);
  v = 2.0;
  EXPECT_EQ(GetAsOptional<int>(v), absl::nullopt);
  EXPECT_EQ(GetAsOptional<double>(v), 2.0);
}

TEST(ResourceAttributionTypeHelpersTest, GetAsOptionalWithMonostate) {
  absl::variant<absl::monostate, int> v;
  EXPECT_EQ(GetAsOptional<int>(v), absl::nullopt);
  v = 1;
  EXPECT_EQ(GetAsOptional<int>(v), 1);
}

// Can't test comparators with absl::variant<> because it can't be
// instantiated.

TEST(ResourceAttributionTypeHelpersTest, VariantComparatorsSingleAlternative) {
  using TestVariant = absl::variant<int>;
  TestVariant v = 1;

  // Make sure the overloaded comparators don't interfere with the default
  // variant comparators.
  EXPECT_EQ(v, absl::variant<int>(1));
  EXPECT_NE(v, absl::variant<int>(-1));
  EXPECT_LT(v, absl::variant<int>(2));
  EXPECT_LE(v, absl::variant<int>(2));
  EXPECT_LE(v, absl::variant<int>(1));
  EXPECT_GT(v, absl::variant<int>(0));
  EXPECT_GE(v, absl::variant<int>(0));
  EXPECT_GE(v, absl::variant<int>(1));

  // Only == and != are defined for variants and their alternative types, and
  // only for variants in the resource_attriution namespace. The rich EXPECT_EQ
  // macros can't be used since they're not in the same namespce.
  EXPECT_TRUE(v == 1);
  EXPECT_FALSE(v == 0);
  EXPECT_TRUE(v != -1);
  EXPECT_FALSE(v != 1);
}

TEST(ResourceAttributionTypeHelpersTest, VariantComparatorsManyAlternatives) {
  using TestVariant = absl::variant<int, double>;
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

}  // namespace performance_manager::resource_attribution::internal
