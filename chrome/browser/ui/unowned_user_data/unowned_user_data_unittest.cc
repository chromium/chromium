// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/unowned_user_data/scoped_unowned_user_data.h"
#include "chrome/browser/ui/unowned_user_data/unowned_user_data_host.h"
#include "chrome/browser/ui/unowned_user_data/unowned_user_data_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestScopedUnownedUserData {
 public:
  DECLARE_USER_DATA(TestScopedUnownedUserData);

  explicit TestScopedUnownedUserData(UnownedUserDataHost& host)
      : scoped_data_holder_(host, *this) {}
  ~TestScopedUnownedUserData() = default;

 private:
  ScopedUnownedUserData<TestScopedUnownedUserData> scoped_data_holder_;
};

DEFINE_USER_DATA(TestScopedUnownedUserData);

class TestUnownedUserDataSubclass
    : public UnownedUserDataInterface<TestUnownedUserDataSubclass> {
 public:
  DECLARE_USER_DATA(TestUnownedUserDataSubclass);

  explicit TestUnownedUserDataSubclass(UnownedUserDataHost& host)
      : UnownedUserDataInterface(host) {}
  ~TestUnownedUserDataSubclass() override = default;
};

DEFINE_USER_DATA(TestUnownedUserDataSubclass);

}  // namespace

// Tests basic functionality of a class with a ScopedUnownedUserData member.
TEST(UnownedUserDataTest, ScopedUnownedUserData) {
  UnownedUserDataHost host;
  EXPECT_EQ(nullptr, TestScopedUnownedUserData::Get(host));
  {
    TestScopedUnownedUserData test_data(host);
    EXPECT_EQ(&test_data, TestScopedUnownedUserData::Get(host));
  }
  EXPECT_EQ(nullptr, TestScopedUnownedUserData::Get(host));
}

// Tests basic functionality of a class inheriting from
// UnownedUserDataInterface.
// TODO(crbug.com/428749341): Re-enable this test for some specific Linux
// builders where it fails consistently.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_UnownedUserDataInterface DISABLED_UnownedUserDataInterface
#else
#define MAYBE_UnownedUserDataInterface UnownedUserDataInterface
#endif  // BUILDFLAG(IS_LINUX)

TEST(UnownedUserDataTest, MAYBE_UnownedUserDataInterface) {
  UnownedUserDataHost host;
  EXPECT_EQ(nullptr, TestUnownedUserDataSubclass::Get(host));
  {
    TestUnownedUserDataSubclass test_data(host);
    EXPECT_EQ(&test_data, TestUnownedUserDataSubclass::Get(host));
  }
  EXPECT_EQ(nullptr, TestUnownedUserDataSubclass::Get(host));
}

// Tests that different types of UnownedUserData are independent of one another
// and can be individually set and unset.
// TODO(crbug.com/428749341): Re-enable this test for some specific Linux
// builders where it fails consistently.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_DifferentTypesAreIndependent DISABLED_DifferentTypesAreIndependent
#else
#define MAYBE_DifferentTypesAreIndependent DifferentTypesAreIndependent
#endif  // BUILDFLAG(IS_LINUX)
TEST(UnownedUserDataTest, MAYBE_DifferentTypesAreIndependent) {
  // We use the two types of TestUnownedUserDataSubclass and
  // TestScopedUnownedUserData. These could be thought of as "UnownedUserData1"
  // and "UnownedUserData2".
  UnownedUserDataHost host;
  // At the start, neither type has an associated entry on the host.
  EXPECT_EQ(nullptr, TestUnownedUserDataSubclass::Get(host));
  EXPECT_EQ(nullptr, TestScopedUnownedUserData::Get(host));

  // Create an entry for TestUnownedUserDataSubclass.
  std::optional<TestUnownedUserDataSubclass> test_data1;
  test_data1.emplace(host);

  // TestUnownedUserDataSubclass should have an associated entry, but
  // TestScopedUnownedUserData should not.
  EXPECT_EQ(&test_data1.value(), TestUnownedUserDataSubclass::Get(host));
  EXPECT_EQ(nullptr, TestScopedUnownedUserData::Get(host));

  // Now, create an entry for TestScopedUnownedUserData.
  std::optional<TestScopedUnownedUserData> test_data2;
  test_data2.emplace(host);

  // Both types should have entries.
  EXPECT_EQ(&test_data1.value(), TestUnownedUserDataSubclass::Get(host));
  EXPECT_EQ(&test_data2.value(), TestScopedUnownedUserData::Get(host));

  // Unset TestUnownedUserDataSubclass. TestScopedUnownedUserData's entry should
  // be unaffected.
  test_data1.reset();
  EXPECT_EQ(nullptr, TestUnownedUserDataSubclass::Get(host));
  EXPECT_EQ(&test_data2.value(), TestScopedUnownedUserData::Get(host));

  // Reset TestScopedUnownedUserData. Both types should now be unset.
  test_data2.reset();
  EXPECT_EQ(nullptr, TestUnownedUserDataSubclass::Get(host));
  EXPECT_EQ(nullptr, TestScopedUnownedUserData::Get(host));
}

// Tests that only one instance of a given type can be set on a host.
TEST(UnownedUserDataTest, CannotSetATypeTwiceOnTheSameHost) {
  UnownedUserDataHost host;
  std::optional<TestScopedUnownedUserData> test_data1;
  std::optional<TestScopedUnownedUserData> test_data2;

  // Construct one test data. This should succeed.
  test_data1.emplace(host);
  // Constructing another data of the same type should cause a CHECK failure.
  EXPECT_DEATH(test_data2.emplace(host), "");
}
