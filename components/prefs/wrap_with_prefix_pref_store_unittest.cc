// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/wrap_with_prefix_pref_store.h"

#include <memory>

#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/prefs/pref_store.h"
#include "components/prefs/testing_pref_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Pointee;

const char kPrefix[] = "prefixed";
const char kTestPref[] = "test.pref";
const char kPrefixedTestPref[] = "prefixed.test.pref";

testing::AssertionResult ValueInStoreIs(const PrefStore& store,
                                        const std::string& path,
                                        const auto& expected) {
  const base::Value expected_value(expected);
  const base::Value* actual_value = nullptr;
  if (!store.GetValue(path, &actual_value)) {
    return testing::AssertionFailure() << "Pref " << path << " isn't present";
  }
  DCHECK(actual_value);
  if (expected_value != *actual_value) {
    return testing::AssertionFailure()
           << "Pref " << path << " has value " << *actual_value
           << " but was expected to be " << expected_value;
  }
  return testing::AssertionSuccess();
}

testing::AssertionResult ValueInStoreIsAbsent(const PrefStore& store,
                                              const std::string& path) {
  const base::Value* actual_value = nullptr;
  if (store.GetValue(path, &actual_value)) {
    DCHECK(actual_value);
    return testing::AssertionFailure()
           << "Pref " << path << " should be absent, but exists with value "
           << *actual_value;
  }
  return testing::AssertionSuccess();
}

class MockPrefStoreObserver : public PrefStore::Observer {
 public:
  ~MockPrefStoreObserver() override = default;

  MOCK_METHOD(void, OnPrefValueChanged, (std::string_view key), (override));
  MOCK_METHOD(void, OnInitializationCompleted, (bool succeeded), (override));
};

class MockReadErrorDelegate : public PersistentPrefStore::ReadErrorDelegate {
 public:
  MOCK_METHOD(void, OnError, (PersistentPrefStore::PrefReadError), (override));
};

class WrapWithPrefixPrefStoreTest : public testing::Test {
 public:
  WrapWithPrefixPrefStoreTest()
      : target_store_(base::MakeRefCounted<TestingPrefStore>()),
        store_(base::MakeRefCounted<WrapWithPrefixPrefStore>(target_store_,
                                                             kPrefix)) {}

  void SetUp() override {
    store_->AddObserver(&observer_);
    target_store_->AddObserver(&target_store_observer_);
  }

  void TearDown() override {
    store_->RemoveObserver(&observer_);
    target_store_->RemoveObserver(&target_store_observer_);
  }

  TestingPrefStore& target_store() { return *target_store_; }
  WrapWithPrefixPrefStore& store() { return *store_; }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<TestingPrefStore> target_store_;
  scoped_refptr<WrapWithPrefixPrefStore> store_;
  NiceMock<MockPrefStoreObserver> observer_;
  NiceMock<MockPrefStoreObserver> target_store_observer_;
};

TEST_F(WrapWithPrefixPrefStoreTest, AddRemoveObserver) {
  MockPrefStoreObserver observer;
  store().AddObserver(&observer);

  EXPECT_CALL(observer, OnPrefValueChanged).Times(0);
  // No observer should be notified since the pref is not prefixed.
  target_store().ReportValueChanged(kTestPref, /*flags=*/0);

  EXPECT_CALL(observer, OnPrefValueChanged(kTestPref));
  target_store().ReportValueChanged(kPrefixedTestPref, /*flags=*/0);

  store().RemoveObserver(&observer);
}

TEST_F(WrapWithPrefixPrefStoreTest, HasObservers) {
  EXPECT_TRUE(store().HasObservers());
  store().RemoveObserver(&observer_);
  EXPECT_FALSE(store().HasObservers());
}

TEST_F(WrapWithPrefixPrefStoreTest, IsInitializationComplete) {
  ASSERT_FALSE(target_store().IsInitializationComplete());
  EXPECT_FALSE(store().IsInitializationComplete());

  EXPECT_CALL(target_store_observer_, OnInitializationCompleted);
  EXPECT_CALL(observer_, OnInitializationCompleted);
  target_store().NotifyInitializationCompleted();

  ASSERT_TRUE(target_store().IsInitializationComplete());
  EXPECT_TRUE(store().IsInitializationComplete());
}

TEST_F(WrapWithPrefixPrefStoreTest, GetValue) {
  ASSERT_TRUE(ValueInStoreIsAbsent(target_store(), kTestPref));
  ASSERT_TRUE(ValueInStoreIsAbsent(target_store(), kPrefixedTestPref));
  ASSERT_TRUE(ValueInStoreIsAbsent(store(), kTestPref));

  target_store().SetString(kTestPref, "value1");
  ASSERT_TRUE(ValueInStoreIs(target_store(), kTestPref, "value1"));
  // kTestPref is not prefixed and should not be returned by `store_`.
  EXPECT_TRUE(ValueInStoreIsAbsent(store(), kTestPref));

  target_store().SetString(kPrefixedTestPref, "value2");
  EXPECT_TRUE(ValueInStoreIs(store(), kTestPref, "value2"));
  EXPECT_TRUE(ValueInStoreIs(target_store(), kTestPref, "value1"));
}

TEST_F(WrapWithPrefixPrefStoreTest, GetValues) {
  ASSERT_THAT(target_store().GetValues(), IsEmpty());
  ASSERT_THAT(store().GetValues(), IsEmpty());

  target_store().SetString(kTestPref, "value1");
  ASSERT_EQ(target_store().GetValues(),
            base::Value::Dict().SetByDottedPath(kTestPref, "value1"));
  EXPECT_THAT(store().GetValues(), IsEmpty());

  target_store().SetString(kPrefixedTestPref, "value2");
  // Expect the new pref store to return the "un-prefixed" value.
  EXPECT_EQ(store().GetValues(),
            base::Value::Dict().SetByDottedPath(kTestPref, "value2"));
  EXPECT_TRUE(ValueInStoreIs(store(), kTestPref, "value2"));
}

TEST_F(WrapWithPrefixPrefStoreTest, SetValue) {
  EXPECT_CALL(observer_, OnPrefValueChanged(kTestPref));
  EXPECT_CALL(target_store_observer_, OnPrefValueChanged(kPrefixedTestPref));
  store().SetValue(kTestPref, base::Value("value"), /*flags=*/0);

  EXPECT_TRUE(ValueInStoreIs(store(), kTestPref, "value"));
  EXPECT_TRUE(ValueInStoreIsAbsent(target_store(), kTestPref));
  // The new pref should be under the prefix dict.
  EXPECT_TRUE(ValueInStoreIs(target_store(), kPrefixedTestPref, "value"));
}

TEST_F(WrapWithPrefixPrefStoreTest, SetValueShouldNotNotifyIfUnchanged) {
  target_store().SetValue(kPrefixedTestPref, base::Value("value"), /*flags=*/0);
  ASSERT_TRUE(ValueInStoreIs(store(), kTestPref, "value"));

  EXPECT_CALL(observer_, OnPrefValueChanged).Times(0);
  store().SetValue(kTestPref, base::Value("value"), /*flags=*/0);
}

TEST_F(WrapWithPrefixPrefStoreTest, SetValueSilently) {
  EXPECT_CALL(observer_, OnPrefValueChanged).Times(0);
  EXPECT_CALL(target_store_observer_, OnPrefValueChanged).Times(0);
  store().SetValueSilently(kTestPref, base::Value("value"), /*flags=*/0);

  EXPECT_TRUE(ValueInStoreIs(store(), kTestPref, "value"));
  EXPECT_TRUE(ValueInStoreIsAbsent(target_store(), kTestPref));
  // The new pref should be under the prefix dict.
  EXPECT_TRUE(ValueInStoreIs(target_store(), kPrefixedTestPref, "value"));
}

TEST_F(WrapWithPrefixPrefStoreTest, GetMutableValue) {
  target_store().SetString(kTestPref, "value1");
  base::Value* value = nullptr;
  // kTestPref is not prefixed and should not be returned by `store_`.
  EXPECT_FALSE(store().GetMutableValue(kTestPref, &value));
  EXPECT_FALSE(value);

  target_store().SetString(kPrefixedTestPref, "value2");
  EXPECT_TRUE(store().GetMutableValue(kTestPref, &value));
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, base::Value("value2"));

  *value = base::Value("value3");
  EXPECT_TRUE(ValueInStoreIs(store(), kTestPref, "value3"));
  EXPECT_TRUE(store().GetMutableValue(kTestPref, &value));
  EXPECT_THAT(value, Pointee(Eq("value3")));

  EXPECT_FALSE(ValueInStoreIsAbsent(target_store(), kTestPref));
}

TEST_F(WrapWithPrefixPrefStoreTest, ReportValueChanged) {
  EXPECT_CALL(observer_, OnPrefValueChanged(kTestPref));
  EXPECT_CALL(target_store_observer_, OnPrefValueChanged(kPrefixedTestPref));
  store().ReportValueChanged(kTestPref, /*flags=*/0);
}

TEST_F(WrapWithPrefixPrefStoreTest, RemoveValue) {
  target_store().SetString(kTestPref, "value1");
  target_store().SetString(kPrefixedTestPref, "value2");

  EXPECT_CALL(observer_, OnPrefValueChanged(kTestPref));
  EXPECT_CALL(target_store_observer_, OnPrefValueChanged(kPrefixedTestPref));
  store().RemoveValue(kTestPref, /*flags=*/0);

  EXPECT_TRUE(ValueInStoreIsAbsent(store(), kTestPref));
  EXPECT_TRUE(ValueInStoreIsAbsent(target_store(), kPrefixedTestPref));
  EXPECT_TRUE(ValueInStoreIs(target_store(), kTestPref, "value1"));
}

TEST_F(WrapWithPrefixPrefStoreTest, RemoveValueShouldNotNotifyIfAbsent) {
  EXPECT_TRUE(ValueInStoreIsAbsent(store(), kTestPref));
  EXPECT_TRUE(ValueInStoreIsAbsent(target_store(), kPrefixedTestPref));

  EXPECT_CALL(observer_, OnPrefValueChanged).Times(0);
  store().RemoveValue(kTestPref, /*flags=*/0);
}

TEST_F(WrapWithPrefixPrefStoreTest, RemoveValuesByPrefixSilently) {
  target_store().SetString("test.pref", "value");
  target_store().SetString("prefixed.test.pref", "value1");
  target_store().SetString("prefixed.test.pref2", "value2");

  ASSERT_EQ(store().GetValues(), base::Value::Dict()
                                     .SetByDottedPath("test.pref", "value1")
                                     .SetByDottedPath("test.pref2", "value2"));

  store().RemoveValuesByPrefixSilently("test");

  EXPECT_THAT(store().GetValues(), IsEmpty());
  EXPECT_EQ(target_store().GetValues(),
            base::Value::Dict().SetByDottedPath("test.pref", "value"));
}

TEST_F(WrapWithPrefixPrefStoreTest, ReadOnly) {
  target_store().set_read_only(false);
  ASSERT_FALSE(target_store().ReadOnly());
  EXPECT_FALSE(store().ReadOnly());

  target_store().set_read_only(true);
  ASSERT_TRUE(target_store().ReadOnly());
  EXPECT_TRUE(store().ReadOnly());
}

TEST_F(WrapWithPrefixPrefStoreTest, GetReadError) {
  ASSERT_EQ(target_store().GetReadError(),
            PersistentPrefStore::PREF_READ_ERROR_NONE);
  EXPECT_EQ(store().GetReadError(), PersistentPrefStore::PREF_READ_ERROR_NONE);

  target_store().set_read_error(
      PersistentPrefStore::PREF_READ_ERROR_JSON_PARSE);
  ASSERT_EQ(target_store().GetReadError(),
            PersistentPrefStore::PREF_READ_ERROR_JSON_PARSE);
  EXPECT_EQ(store().GetReadError(),
            PersistentPrefStore::PREF_READ_ERROR_JSON_PARSE);
}

TEST_F(WrapWithPrefixPrefStoreTest, ReadPrefsForwardsReadError) {
  // Read error.
  target_store().set_read_error(
      PersistentPrefStore::PREF_READ_ERROR_JSON_PARSE);

  ASSERT_EQ(target_store().ReadPrefs(),
            PersistentPrefStore::PREF_READ_ERROR_JSON_PARSE);
  // Should now forward the read error.
  EXPECT_EQ(store().ReadPrefs(),
            PersistentPrefStore::PREF_READ_ERROR_JSON_PARSE);
}

TEST_F(WrapWithPrefixPrefStoreTest, ReadPrefsForwardsReadSuccess) {
  // Read success.
  target_store().set_read_error(PersistentPrefStore::PREF_READ_ERROR_NONE);
  ASSERT_EQ(target_store().ReadPrefs(),
            PersistentPrefStore::PREF_READ_ERROR_NONE);
  // Should now forward the read success.
  EXPECT_EQ(store().ReadPrefs(), PersistentPrefStore::PREF_READ_ERROR_NONE);
}

TEST_F(WrapWithPrefixPrefStoreTest,
       ReadPrefsAsyncUponPrexistingReadPrefsSuccess) {
  ASSERT_EQ(target_store().ReadPrefs(),
            PersistentPrefStore::PREF_READ_ERROR_NONE);
  ASSERT_TRUE(target_store().IsInitializationComplete());

  // The callee is expected to take the ownership, hence the assignment to a raw
  // ptr.
  auto* read_error_delegate =
      new ::testing::StrictMock<MockReadErrorDelegate>();
  EXPECT_CALL(*read_error_delegate, OnError).Times(0);
  store().ReadPrefsAsync(read_error_delegate);
}

TEST_F(WrapWithPrefixPrefStoreTest,
       ReadPrefsAsyncUponPrexistingReadPrefsAsyncSuccess) {
  target_store().ReadPrefsAsync(nullptr);
  ASSERT_TRUE(target_store().IsInitializationComplete());

  // The callee is expected to take the ownership, hence the assignment to a raw
  // ptr.
  auto* read_error_delegate =
      new ::testing::StrictMock<MockReadErrorDelegate>();
  EXPECT_CALL(*read_error_delegate, OnError).Times(0);
  store().ReadPrefsAsync(read_error_delegate);
}

TEST_F(WrapWithPrefixPrefStoreTest,
       ReadPrefsAsyncForwardsPreexistingReadError) {
  target_store().SetBlockAsyncRead(true);
  target_store().ReadPrefsAsync(nullptr);
  target_store().set_read_error(
      PersistentPrefStore::PREF_READ_ERROR_ACCESS_DENIED);
  target_store().SetBlockAsyncRead(false);

  // The callee is expected to take the ownership, hence the assignment to a raw
  // ptr.
  auto* read_error_delegate =
      new ::testing::StrictMock<MockReadErrorDelegate>();
  EXPECT_CALL(*read_error_delegate,
              OnError(PersistentPrefStore::PREF_READ_ERROR_ACCESS_DENIED));
  store().ReadPrefsAsync(read_error_delegate);
}

TEST_F(WrapWithPrefixPrefStoreTest,
       ReadPrefsAsyncUponUnderlyingReadPrefsAsyncSuccess) {
  target_store().SetBlockAsyncRead(true);
  target_store().ReadPrefsAsync(nullptr);

  // The callee is expected to take the ownership, hence the assignment to a raw
  // ptr.
  auto* read_error_delegate =
      new ::testing::StrictMock<MockReadErrorDelegate>();
  EXPECT_CALL(*read_error_delegate, OnError).Times(0);
  store().ReadPrefsAsync(read_error_delegate);
  ASSERT_FALSE(store().IsInitializationComplete());

  target_store().SetBlockAsyncRead(false);
  ASSERT_TRUE(store().IsInitializationComplete());
}

TEST_F(WrapWithPrefixPrefStoreTest, ReadPrefsAsyncForwardsReadError) {
  target_store().SetBlockAsyncRead(true);
  target_store().ReadPrefsAsync(nullptr);

  // The callee is expected to take the ownership, hence the assignment to a raw
  // ptr.
  auto* read_error_delegate =
      new ::testing::StrictMock<MockReadErrorDelegate>();
  EXPECT_CALL(*read_error_delegate, OnError).Times(0);
  store().ReadPrefsAsync(read_error_delegate);

  target_store().set_read_error(
      PersistentPrefStore::PREF_READ_ERROR_ACCESS_DENIED);

  EXPECT_CALL(*read_error_delegate,
              OnError(PersistentPrefStore::PREF_READ_ERROR_ACCESS_DENIED));
  target_store().SetBlockAsyncRead(false);
}

TEST_F(WrapWithPrefixPrefStoreTest, HasReadErrorDelegate) {
  EXPECT_FALSE(store().HasReadErrorDelegate());

  // Target store's ReadPrefsAsync() is a prerequisite.
  target_store().ReadPrefsAsync(nullptr);
  store().ReadPrefsAsync(new MockReadErrorDelegate);
  EXPECT_TRUE(store().HasReadErrorDelegate());
}

TEST_F(WrapWithPrefixPrefStoreTest, HasReadErrorDelegateWithNullDelegate) {
  EXPECT_FALSE(store().HasReadErrorDelegate());

  // Target store's ReadPrefsAsync() is a prerequisite.
  target_store().ReadPrefsAsync(nullptr);
  store().ReadPrefsAsync(nullptr);
  // Returns true even though no instance was passed.
  EXPECT_TRUE(store().HasReadErrorDelegate());
}

TEST_F(WrapWithPrefixPrefStoreTest, GetValueForPrefixedKeyIfNonExisting) {
  target_store().SetString(kPrefixedTestPref, "value");
  EXPECT_FALSE(store().GetValue(kPrefixedTestPref, nullptr));
}

TEST_F(WrapWithPrefixPrefStoreTest, GetValueForExistingIfExisting) {
  target_store().SetString("prefixed.prefixed.test.pref", "value");
  EXPECT_TRUE(ValueInStoreIs(store(), kPrefixedTestPref, "value"));
}

TEST_F(WrapWithPrefixPrefStoreTest, SetValueForPrefixedKey) {
  EXPECT_CALL(observer_, OnPrefValueChanged(kPrefixedTestPref));

  store().SetValue(kPrefixedTestPref, base::Value("value"), /*flags=*/0);
  EXPECT_TRUE(ValueInStoreIs(store(), kPrefixedTestPref, "value"));
}

using WrapWithPrefixPrefStoreDeathTest = WrapWithPrefixPrefStoreTest;

TEST_F(WrapWithPrefixPrefStoreDeathTest,
       ReadPrefsShouldCrashIfUnderlyingStoreUninitialized) {
  ASSERT_FALSE(target_store().IsInitializationComplete());
  // Disallowed to call ReadPrefs() without the underlying store having been
  // initialized.
  EXPECT_CHECK_DEATH(store().ReadPrefs());
}

TEST_F(WrapWithPrefixPrefStoreDeathTest,
       ReadPrefsAsyncShouldCrashIfUnderlyingStoreUninitialized) {
  ASSERT_FALSE(target_store().IsInitializationComplete());
  // Disallowed to call ReadPrefs() without the underlying store having been
  // initialized.
  EXPECT_CHECK_DEATH(store().ReadPrefsAsync(nullptr));
}

}  // namespace
