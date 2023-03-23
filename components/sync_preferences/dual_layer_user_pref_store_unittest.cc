// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/dual_layer_user_pref_store.h"

#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/testing_pref_store.h"
#include "components/sync_preferences/syncable_prefs_database.h"
#include "components/sync_preferences/test_syncable_prefs_database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_preferences {

namespace {

constexpr char kPref1[] = "pref1";
constexpr char kPref2[] = "pref2";
constexpr char kPref3[] = "pref3";
constexpr char kPrefName[] = "pref";
constexpr char kPriorityPrefName[] = "priority-pref";
constexpr char kNonExistentPrefName[] = "nonexistent-pref";
constexpr char kNonSyncablePrefName[] = "nonsyncable-pref";

// Assigning an id of 0 to all the test prefs.
const std::unordered_map<std::string, SyncablePrefMetadata>
    kSyncablePrefsDatabase = {
        {kPref1, {0, syncer::PREFERENCES}},
        {kPref2, {0, syncer::PREFERENCES}},
        {kPref3, {0, syncer::PREFERENCES}},
        {kPrefName, {0, syncer::PREFERENCES}},
        {kPriorityPrefName, {0, syncer::PRIORITY_PREFERENCES}},
};

base::Value MakeDict(
    const std::vector<std::pair<std::string, std::string>>& values) {
  base::Value::Dict dict;
  for (const auto& [key, value] : values) {
    dict.Set(key, value);
  }
  return base::Value(std::move(dict));
}

testing::AssertionResult ValueInStoreIs(const PrefStore& store,
                                        const std::string& pref,
                                        const base::Value& expected_value) {
  const base::Value* actual_value = nullptr;
  if (!store.GetValue(pref, &actual_value)) {
    return testing::AssertionFailure() << "Pref " << pref << " isn't present";
  }
  DCHECK(actual_value);
  if (expected_value != *actual_value) {
    return testing::AssertionFailure()
           << "Pref " << pref << " has value " << *actual_value
           << " but was expected to be " << expected_value;
  }
  return testing::AssertionSuccess();
}

testing::AssertionResult ValueInStoreIs(const PrefStore& store,
                                        const std::string& pref,
                                        const std::string& expected_value) {
  return ValueInStoreIs(store, pref, base::Value(expected_value));
}

testing::AssertionResult ValueInStoreIsAbsent(const PrefStore& store,
                                              const std::string& pref) {
  const base::Value* actual_value = nullptr;
  if (store.GetValue(pref, &actual_value)) {
    DCHECK(actual_value);
    return testing::AssertionFailure()
           << "Pref " << pref << " should be absent, but exists with value "
           << *actual_value;
  }
  return testing::AssertionSuccess();
}

class MockPrefStoreObserver : public PrefStore::Observer {
 public:
  ~MockPrefStoreObserver() override = default;

  MOCK_METHOD(void, OnPrefValueChanged, (const std::string& key), (override));
  MOCK_METHOD(void, OnInitializationCompleted, (bool succeeded), (override));
};

}  // namespace

class DualLayerUserPrefStoreTestBase : public testing::Test {
 public:
  explicit DualLayerUserPrefStoreTestBase(bool initialize)
      : syncable_prefs_database_(kSyncablePrefsDatabase) {
    local_store_ = base::MakeRefCounted<TestingPrefStore>();
    dual_layer_store_ = base::MakeRefCounted<DualLayerUserPrefStore>(
        local_store_, &syncable_prefs_database_);

    if (initialize) {
      local_store_->NotifyInitializationCompleted();
    }
  }

  TestingPrefStore* local_store() { return local_store_.get(); }
  DualLayerUserPrefStore* store() { return dual_layer_store_.get(); }

 protected:
  scoped_refptr<TestingPrefStore> local_store_;
  scoped_refptr<DualLayerUserPrefStore> dual_layer_store_;
  TestSyncablePrefsDatabase syncable_prefs_database_;
};

class DualLayerUserPrefStoreTest : public DualLayerUserPrefStoreTestBase {
 public:
  DualLayerUserPrefStoreTest() : DualLayerUserPrefStoreTestBase(true) {
    // TODO(crbug.com/1416480): Add proper test setup to enable and disable data
    // types appropriately.
    dual_layer_store_->EnableType(syncer::PREFERENCES);
    dual_layer_store_->EnableType(syncer::PRIORITY_PREFERENCES);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    dual_layer_store_->EnableType(syncer::OS_PREFERENCES);
    dual_layer_store_->EnableType(syncer::OS_PRIORITY_PREFERENCES);
#endif
  }
};

class DualLayerUserPrefStoreInitializationTest
    : public DualLayerUserPrefStoreTestBase {
 public:
  DualLayerUserPrefStoreInitializationTest()
      : DualLayerUserPrefStoreTestBase(false) {}
};

TEST_F(DualLayerUserPrefStoreInitializationTest,
       ForwardsInitializationSuccess) {
  // The account store (an in-memory store) always starts out already
  // initialized, but the local store is *not* initialized yet.
  ASSERT_FALSE(local_store()->IsInitializationComplete());
  ASSERT_TRUE(store()->GetAccountPrefStore()->IsInitializationComplete());

  // Accordingly, the dual-layer store is not initialized either.
  EXPECT_FALSE(store()->IsInitializationComplete());

  MockPrefStoreObserver observer;
  store()->AddObserver(&observer);

  // Once the local store is successfully initialized, so it the dual-layer
  // store.
  EXPECT_CALL(observer, OnInitializationCompleted(true));
  local_store()->NotifyInitializationCompleted();

  EXPECT_TRUE(store()->IsInitializationComplete());
  EXPECT_EQ(store()->GetReadError(), PersistentPrefStore::PREF_READ_ERROR_NONE);

  store()->RemoveObserver(&observer);
}

TEST_F(DualLayerUserPrefStoreInitializationTest,
       ForwardsInitializationFailure) {
  // The account store (an in-memory store) always starts out already
  // initialized, but the local store is *not* initialized yet.
  ASSERT_FALSE(local_store()->IsInitializationComplete());
  ASSERT_TRUE(store()->GetAccountPrefStore()->IsInitializationComplete());

  // Accordingly, the dual-layer store is not initialized either.
  EXPECT_FALSE(store()->IsInitializationComplete());

  MockPrefStoreObserver observer;
  store()->AddObserver(&observer);

  // The local store encounters some read error.
  local_store()->set_read_error(
      PersistentPrefStore::PREF_READ_ERROR_JSON_PARSE);
  local_store()->set_read_success(false);

  // Once the local store reports the error, the dual-layer store should forward
  // it accordingly.
  EXPECT_CALL(observer, OnInitializationCompleted(false));
  local_store()->NotifyInitializationCompleted();

  EXPECT_TRUE(store()->IsInitializationComplete());
  EXPECT_EQ(store()->GetReadError(),
            PersistentPrefStore::PREF_READ_ERROR_JSON_PARSE);

  store()->RemoveObserver(&observer);
}

TEST_F(DualLayerUserPrefStoreTest, ReadsFromLocalStore) {
  store()->GetLocalPrefStore()->SetValueSilently(kPrefName,
                                                 base::Value("local_value"), 0);

  // No value is set in the account store, so the one from the local store
  // should be returned.
  EXPECT_TRUE(ValueInStoreIs(*store(), kPrefName, "local_value"));

  // Reading the value should not have affected the account store.
  EXPECT_TRUE(ValueInStoreIsAbsent(*store()->GetAccountPrefStore(), kPrefName));
}

TEST_F(DualLayerUserPrefStoreTest, ReadsFromAccountStore) {
  store()->GetAccountPrefStore()->SetValueSilently(
      kPrefName, base::Value("account_value"), 0);

  // No value is set in the local store, so the one from the account store
  // should be returned.
  EXPECT_TRUE(ValueInStoreIs(*store(), kPrefName, "account_value"));

  // Reading the value should not have affected the local store.
  EXPECT_TRUE(ValueInStoreIsAbsent(*store()->GetLocalPrefStore(), kPrefName));
}

TEST_F(DualLayerUserPrefStoreTest, AccountTakesPrecedence) {
  store()->GetAccountPrefStore()->SetValueSilently(
      kPrefName, base::Value("account_value"), 0);
  store()->GetLocalPrefStore()->SetValueSilently(kPrefName,
                                                 base::Value("local_value"), 0);

  // Different values are set in both stores; the one from the account should
  // take precedence.
  EXPECT_TRUE(ValueInStoreIs(*store(), kPrefName, "account_value"));
}

TEST_F(DualLayerUserPrefStoreTest, ReadsFromBothStores) {
  // Three prefs: One is set in both stores, one only in the local store, and
  // one only in the account store.
  store()->GetLocalPrefStore()->SetValueSilently(
      kPref1, base::Value("local_value1"), 0);
  store()->GetAccountPrefStore()->SetValueSilently(
      kPref1, base::Value("account_value1"), 0);
  store()->GetLocalPrefStore()->SetValueSilently(
      kPref2, base::Value("local_value2"), 0);
  store()->GetAccountPrefStore()->SetValueSilently(
      kPref3, base::Value("account_value3"), 0);

  base::Value::Dict expected_values;
  // For the pref that exists in both stores, the account value should take
  // precedence.
  expected_values.Set(kPref1, "account_value1");
  // For the prefs that only exist in one store, their value should be returned.
  expected_values.Set(kPref2, "local_value2");
  expected_values.Set(kPref3, "account_value3");
  EXPECT_EQ(store()->GetValues(), expected_values);
}

TEST_F(DualLayerUserPrefStoreTest, WritesToBothStores) {
  // Three prefs: One is set in both stores, one only in the local store, and
  // one only in the account store.
  store()->GetLocalPrefStore()->SetValueSilently(
      kPref1, base::Value("local_value1"), 0);
  store()->GetAccountPrefStore()->SetValueSilently(
      kPref1, base::Value("account_value1"), 0);
  store()->GetLocalPrefStore()->SetValueSilently(
      kPref2, base::Value("local_value2"), 0);
  store()->GetAccountPrefStore()->SetValueSilently(
      kPref3, base::Value("account_value3"), 0);

  // Update all three prefs.
  store()->SetValue(kPref1, base::Value("new_value1"), 0);
  store()->SetValue(kPref2, base::Value("new_value2"), 0);
  store()->SetValue(kPref3, base::Value("new_value3"), 0);

  // The new values should be returned from the dual-layer store...
  ASSERT_TRUE(ValueInStoreIs(*store(), kPref1, "new_value1"));
  ASSERT_TRUE(ValueInStoreIs(*store(), kPref2, "new_value2"));
  ASSERT_TRUE(ValueInStoreIs(*store(), kPref3, "new_value3"));
  // ...but should also be stored in both the local and the account store.
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetLocalPrefStore(), kPref1, "new_value1"));
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetLocalPrefStore(), kPref2, "new_value2"));
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetLocalPrefStore(), kPref3, "new_value3"));
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetAccountPrefStore(), kPref1, "new_value1"));
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetAccountPrefStore(), kPref2, "new_value2"));
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetAccountPrefStore(), kPref3, "new_value3"));
}

TEST_F(DualLayerUserPrefStoreTest, RemovesFromBothStores) {
  // Three prefs: One is set in both stores, one only in the local store, and
  // one only in the account store.
  store()->GetLocalPrefStore()->SetValueSilently(
      kPref1, base::Value("local_value1"), 0);
  store()->GetAccountPrefStore()->SetValueSilently(
      kPref1, base::Value("account_value1"), 0);
  store()->GetLocalPrefStore()->SetValueSilently(
      kPref2, base::Value("local_value2"), 0);
  store()->GetAccountPrefStore()->SetValueSilently(
      kPref3, base::Value("account_value3"), 0);

  // Remove all three prefs.
  store()->RemoveValue(kPref1, 0);
  store()->RemoveValue(kPref2, 0);
  store()->RemoveValue(kPref3, 0);

  // The values should now be absent from the dual-layer store...
  ASSERT_TRUE(ValueInStoreIsAbsent(*store(), kPref1));
  ASSERT_TRUE(ValueInStoreIsAbsent(*store(), kPref2));
  ASSERT_TRUE(ValueInStoreIsAbsent(*store(), kPref3));
  // ...as well as from both the local and the account store.
  EXPECT_TRUE(ValueInStoreIsAbsent(*store()->GetLocalPrefStore(), kPref1));
  EXPECT_TRUE(ValueInStoreIsAbsent(*store()->GetLocalPrefStore(), kPref2));
  EXPECT_TRUE(ValueInStoreIsAbsent(*store()->GetLocalPrefStore(), kPref3));
  EXPECT_TRUE(ValueInStoreIsAbsent(*store()->GetAccountPrefStore(), kPref1));
  EXPECT_TRUE(ValueInStoreIsAbsent(*store()->GetAccountPrefStore(), kPref2));
  EXPECT_TRUE(ValueInStoreIsAbsent(*store()->GetAccountPrefStore(), kPref3));
}

TEST_F(DualLayerUserPrefStoreTest, DoesNotReturnNonexistentPref) {
  store()->SetValueSilently(kPrefName, MakeDict({{"key", "value"}}), 0);

  // The existing pref can be queried.
  ASSERT_TRUE(store()->GetValue(kPrefName, nullptr));
  ASSERT_TRUE(store()->GetMutableValue(kPrefName, nullptr));

  // But a non-existing pref can't.
  EXPECT_FALSE(store()->GetValue(kNonExistentPrefName, nullptr));
  EXPECT_FALSE(store()->GetMutableValue(kNonExistentPrefName, nullptr));
}

TEST_F(DualLayerUserPrefStoreTest, WritesMutableValueFromLocalToBothStores) {
  const base::Value original_value = MakeDict({{"key", "value"}});

  // A dictionary-type value is present in the local store.
  store()->GetLocalPrefStore()->SetValueSilently(kPrefName,
                                                 original_value.Clone(), 0);

  // GetMutableValue() should return that value. In practice, this API is used
  // by ScopedDictPrefUpdate and ScopedListPrefUpdate.
  base::Value* mutable_value = nullptr;
  ASSERT_TRUE(store()->GetMutableValue(kPrefName, &mutable_value));
  ASSERT_TRUE(mutable_value);
  ASSERT_EQ(*mutable_value, original_value);

  // Update it!
  mutable_value->SetStringKey("key", "new_value");

  const base::Value expected_value = mutable_value->Clone();

  // After updating the value, clients have to call ReportValueChanged() to let
  // the store know it has changed. The dual-layer store uses this to reconcile
  // between the two underlying stores.
  store()->ReportValueChanged(kPrefName, 0);

  // The new value should of course be returned from the dual-layer store now,
  // but it should also have been written to both of the underlying stores.
  ASSERT_TRUE(ValueInStoreIs(*store(), kPrefName, expected_value));
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetLocalPrefStore(), kPrefName, expected_value));
  EXPECT_TRUE(ValueInStoreIs(*store()->GetAccountPrefStore(), kPrefName,
                             expected_value));
}

TEST_F(DualLayerUserPrefStoreTest, WritesMutableValueFromAccountToBothStores) {
  const base::Value original_value = MakeDict({{"key", "value"}});

  // A dictionary-type value is present in the account store.
  store()->GetAccountPrefStore()->SetValueSilently(kPrefName,
                                                   original_value.Clone(), 0);

  // GetMutableValue() should return that value. In practice, this API is used
  // by ScopedDictPrefUpdate and ScopedListPrefUpdate.
  base::Value* mutable_value = nullptr;
  ASSERT_TRUE(store()->GetMutableValue(kPrefName, &mutable_value));
  ASSERT_TRUE(mutable_value);
  ASSERT_EQ(*mutable_value, original_value);

  // Update it!
  mutable_value->SetStringKey("key", "new_value");

  const base::Value expected_value = mutable_value->Clone();

  // After updating the value, clients have to call ReportValueChanged() to let
  // the store know it has changed. The dual-layer store uses this to reconcile
  // between the two underlying stores.
  store()->ReportValueChanged(kPrefName, 0);

  // The new value should of course be returned from the dual-layer store now,
  // but it should also have been written to both of the underlying stores.
  ASSERT_TRUE(ValueInStoreIs(*store(), kPrefName, expected_value));
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetLocalPrefStore(), kPrefName, expected_value));
  EXPECT_TRUE(ValueInStoreIs(*store()->GetAccountPrefStore(), kPrefName,
                             expected_value));
}

TEST_F(DualLayerUserPrefStoreTest, WritesMutableValueFromBothToBothStores) {
  const base::Value original_local_value = MakeDict({{"key", "local_value"}});
  const base::Value original_account_value =
      MakeDict({{"key", "account_value"}});

  // A dictionary-type value is present in both of the underlying stores.
  store()->GetLocalPrefStore()->SetValueSilently(
      kPrefName, original_local_value.Clone(), 0);
  store()->GetAccountPrefStore()->SetValueSilently(
      kPrefName, original_account_value.Clone(), 0);

  // GetMutableValue() should return that value. In practice, this API is used
  // by ScopedDictPrefUpdate and ScopedListPrefUpdate.
  base::Value* mutable_value = nullptr;
  ASSERT_TRUE(store()->GetMutableValue(kPrefName, &mutable_value));
  ASSERT_TRUE(mutable_value);
  ASSERT_EQ(*mutable_value, original_account_value);

  // Update it!
  mutable_value->SetStringKey("key", "new_value");

  const base::Value expected_value = mutable_value->Clone();

  // After updating the value, clients have to call ReportValueChanged() to let
  // the store know it has changed. The dual-layer store uses this to reconcile
  // between the two underlying stores.
  store()->ReportValueChanged(kPrefName, 0);

  // The new value should of course be returned from the dual-layer store now,
  // but it should also have been written to both of the underlying stores.
  ASSERT_TRUE(ValueInStoreIs(*store(), kPrefName, expected_value));
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetLocalPrefStore(), kPrefName, expected_value));
  EXPECT_TRUE(ValueInStoreIs(*store()->GetAccountPrefStore(), kPrefName,
                             expected_value));
}

TEST_F(DualLayerUserPrefStoreTest, ClearsMutableValueFromBothStores) {
  // A dictionary-type value is present in both of the underlying stores.
  const base::Value original_value = MakeDict({{"key", "value"}});
  store()->SetValueSilently(kPrefName, original_value.Clone(), 0);
  ASSERT_TRUE(
      ValueInStoreIs(*store()->GetLocalPrefStore(), kPrefName, original_value));
  ASSERT_TRUE(ValueInStoreIs(*store()->GetAccountPrefStore(), kPrefName,
                             original_value));

  // GetMutableValue() should return that value. In practice, this API is used
  // by ScopedDictPrefUpdate and ScopedListPrefUpdate.
  base::Value* mutable_value = nullptr;
  ASSERT_TRUE(store()->GetMutableValue(kPrefName, &mutable_value));
  ASSERT_TRUE(mutable_value);
  ASSERT_EQ(*mutable_value, original_value);

  mutable_value->SetStringKey("key", "new_value");

  // While the mutable value is "pending" (hasn't been "released" via
  // ReportValueChanged()), the pref gets cleared.
  // This shouldn't usually happen in practice, but in theory it could.
  store()->RemoveValue(kPrefName, 0);

  // Now the client that called GetMutableValue() previously reports that it is
  // done changing the value.
  store()->ReportValueChanged(kPrefName, 0);

  // The value should have been removed from both of the stores.
  EXPECT_TRUE(ValueInStoreIsAbsent(*store(), kPrefName));
  EXPECT_TRUE(ValueInStoreIsAbsent(*store()->GetLocalPrefStore(), kPrefName));
  EXPECT_TRUE(ValueInStoreIsAbsent(*store()->GetAccountPrefStore(), kPrefName));
}

TEST_F(DualLayerUserPrefStoreTest, NotifiesOfPrefChanges) {
  // Three prefs: One is set in both stores, one only in the local store, and
  // one only in the account store.
  store()->GetLocalPrefStore()->SetValueSilently(
      kPref1, base::Value("local_value1"), 0);
  store()->GetAccountPrefStore()->SetValueSilently(
      kPref1, base::Value("account_value1"), 0);
  store()->GetLocalPrefStore()->SetValueSilently(
      kPref2, base::Value("local_value2"), 0);
  store()->GetAccountPrefStore()->SetValueSilently(
      kPref3, base::Value("account_value3"), 0);

  testing::StrictMock<MockPrefStoreObserver> observer;
  store()->AddObserver(&observer);

  // Update the prefs. In each case, there should be exactly one pref-change
  // notification.
  EXPECT_CALL(observer, OnPrefValueChanged(kPref1));
  EXPECT_CALL(observer, OnPrefValueChanged(kPref2));
  EXPECT_CALL(observer, OnPrefValueChanged(kPref3));
  store()->SetValue(kPref1, base::Value("new_value1"), 0);
  store()->SetValue(kPref2, base::Value("new_value2"), 0);
  store()->SetValue(kPref3, base::Value("new_value3"), 0);

  // Remove the prefs. Again, there should be one notification each.
  EXPECT_CALL(observer, OnPrefValueChanged(kPref1));
  EXPECT_CALL(observer, OnPrefValueChanged(kPref2));
  EXPECT_CALL(observer, OnPrefValueChanged(kPref3));
  store()->RemoveValue(kPref1, 0);
  store()->RemoveValue(kPref2, 0);
  store()->RemoveValue(kPref3, 0);

  store()->RemoveObserver(&observer);
}

TEST_F(DualLayerUserPrefStoreTest,
       NotifiesOfPrefChangesOnlyIfEffectiveValueChanges) {
  testing::StrictMock<MockPrefStoreObserver> observer;
  store()->AddObserver(&observer);

  // Add a pref to both stores but with different values.
  store()->GetLocalPrefStore()->SetValueSilently(
      kPref1, base::Value("local_value1"), 0);
  store()->GetAccountPrefStore()->SetValueSilently(
      kPref1, base::Value("account_value1"), 0);

  // Should not lead to a notification since the effective value hasn't changed.
  store()->SetValue(kPref1, base::Value("account_value1"), 0);
  // But should still update the local pref store.
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetLocalPrefStore(), kPref1, "account_value1"));

  // Add a pref to the local store only.
  store()->GetLocalPrefStore()->SetValueSilently(
      kPref2, base::Value("local_value2"), 0);

  // Should not lead to a notification since the effective value hasn't changed.
  store()->SetValue(kPref2, base::Value("local_value2"), 0);
  // But should still update the account pref store.
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetAccountPrefStore(), kPref2, "local_value2"));

  // Add a pref to the account store only.
  store()->GetAccountPrefStore()->SetValueSilently(
      kPref3, base::Value("account_value3"), 0);

  // Should not lead to a notification since the effective value hasn't changed.
  store()->SetValue(kPref3, base::Value("account_value3"), 0);
  // But should still update the local pref store.
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetLocalPrefStore(), kPref3, "account_value3"));

  // Add the same pref to both stores.
  store()->SetValueSilently(kPrefName, base::Value("value"), 0);

  EXPECT_CALL(observer, OnPrefValueChanged(kPrefName));
  // Effective value changes, so expect a notification.
  store()->SetValue(kPrefName, base::Value("new_value"), 0);

  store()->RemoveObserver(&observer);
}

TEST_F(DualLayerUserPrefStoreTest, NotifiesOfPrefChangesInUnderlyingStores) {
  // Three prefs: One is set in both stores, one only in the local store, and
  // one only in the account store.
  store()->GetLocalPrefStore()->SetValueSilently(kPref1,
                                                 base::Value("local_value"), 0);
  store()->GetAccountPrefStore()->SetValueSilently(
      kPref2, base::Value("account_value"), 0);

  testing::StrictMock<MockPrefStoreObserver> observer;
  store()->AddObserver(&observer);

  // Update the prefs by writing directly to the underlying stores. (For the
  // account store, that happens when a pref is updated from Sync. For the local
  // store, it shouldn't happen in practice.)
  // The dual-layer store should notify about these changes.
  EXPECT_CALL(observer, OnPrefValueChanged(kPref1));
  EXPECT_CALL(observer, OnPrefValueChanged(kPref2));
  store()->GetLocalPrefStore()->SetValue(kPref1, base::Value("new_value1"), 0);
  store()->GetAccountPrefStore()->SetValue(kPref2, base::Value("new_value2"),
                                           0);

  // Same with removals directly in the underlying stores.
  EXPECT_CALL(observer, OnPrefValueChanged(kPref1));
  EXPECT_CALL(observer, OnPrefValueChanged(kPref2));
  store()->GetLocalPrefStore()->RemoveValue(kPref1, 0);
  store()->GetAccountPrefStore()->RemoveValue(kPref2, 0);

  // TODO(crbug.com/1416477): Verify that OnPrefValueChanged() only gets called
  // when the *effective* value changes, i.e. not when a pref is changed in the
  // local store that also has a value in the account store. (Though this
  // shouldn't happen in practice anyway.)

  store()->RemoveObserver(&observer);
}

TEST_F(DualLayerUserPrefStoreTest,
       NotifiesOfPrefChangesInUnderlyingStoresOnlyIfEffectiveValueChanges) {
  // Two prefs: One is set only in the local store, the other set in both
  // stores.
  store()->GetLocalPrefStore()->SetValueSilently(
      kPref1, base::Value("local_value1"), 0);
  store()->GetLocalPrefStore()->SetValueSilently(
      kPref2, base::Value("local_value2"), 0);
  store()->GetAccountPrefStore()->SetValueSilently(
      kPref2, base::Value("account_value2"), 0);

  testing::StrictMock<MockPrefStoreObserver> observer;
  store()->AddObserver(&observer);

  // Update the prefs by writing directly to the underlying stores.
  // The dual-layer store should notify about these changes only when the
  // the *effective* value changes, i.e. not when a pref is changed in the
  // local store that also has a value in the account store.
  EXPECT_CALL(observer, OnPrefValueChanged(kPref1));
  store()->GetLocalPrefStore()->SetValue(kPref1, base::Value("new_value1"), 0);
  // Should not lead to a notification since the effective value has not
  // changed.
  store()->GetLocalPrefStore()->SetValue(kPref2, base::Value("new_value2"), 0);

  // Same with removals directly in the underlying stores.
  EXPECT_CALL(observer, OnPrefValueChanged(kPref1));
  store()->GetLocalPrefStore()->RemoveValue(kPref1, 0);
  store()->GetLocalPrefStore()->RemoveValue(kPref2, 0);

  store()->RemoveObserver(&observer);
}

TEST_F(DualLayerUserPrefStoreTest, NotifiesOfRemoveOnlyIfPrefExists) {
  // Add a single pref.
  store()->SetValueSilently(kPref1, base::Value("value"), 0);

  testing::StrictMock<MockPrefStoreObserver> observer;
  store()->AddObserver(&observer);

  // Only the added pref should raise a notification.
  EXPECT_CALL(observer, OnPrefValueChanged(kPref1));
  store()->RemoveValue(kPref1, 0);
  // `kPref2` was not added and should not raise any notification.
  store()->RemoveValue(kPref2, 0);

  store()->RemoveObserver(&observer);
}

TEST_F(DualLayerUserPrefStoreTest, NotifiesOfMutableValuePrefChanges) {
  // Three dictionary-valued prefs: One is set in both stores, one only in the
  // local store, and one only in the account store.
  store()->GetLocalPrefStore()->SetValueSilently(
      kPref1, MakeDict({{"key1", "local_value1"}}), 0);
  store()->GetAccountPrefStore()->SetValueSilently(
      kPref1, MakeDict({{"key1", "account_value1"}}), 0);
  store()->GetLocalPrefStore()->SetValueSilently(
      kPref2, MakeDict({{"key2", "local_value2"}}), 0);
  store()->GetAccountPrefStore()->SetValueSilently(
      kPref3, MakeDict({{"key3", "account_value3"}}), 0);

  testing::StrictMock<MockPrefStoreObserver> observer;
  store()->AddObserver(&observer);

  // Update the prefs via GetMutableValue() + ReportValueChanged(). In each
  // case, there should be exactly one pref-change notification.
  EXPECT_CALL(observer, OnPrefValueChanged(kPref1));
  EXPECT_CALL(observer, OnPrefValueChanged(kPref2));
  EXPECT_CALL(observer, OnPrefValueChanged(kPref3));

  base::Value* value1 = nullptr;
  ASSERT_TRUE(store()->GetMutableValue(kPref1, &value1));
  value1->SetStringKey("key1", "new_value1");
  store()->ReportValueChanged(kPref1, 0);

  base::Value* value2 = nullptr;
  ASSERT_TRUE(store()->GetMutableValue(kPref2, &value2));
  value2->SetStringKey("key2", "new_value2");
  store()->ReportValueChanged(kPref2, 0);

  base::Value* value3 = nullptr;
  ASSERT_TRUE(store()->GetMutableValue(kPref3, &value3));
  value3->SetStringKey("key3", "new_value3");
  store()->ReportValueChanged(kPref3, 0);

  store()->RemoveObserver(&observer);
}

TEST_F(DualLayerUserPrefStoreTest, ShouldAddOnlySyncablePrefsToAccountStore) {
  constexpr char kNewValue[] = "new_value";

  store()->SetValue(kPrefName, base::Value(kNewValue), 0);

  // Value should be set in both the stores.
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetLocalPrefStore(), kPrefName, kNewValue));
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetAccountPrefStore(), kPrefName, kNewValue));

  store()->SetValue(kNonSyncablePrefName, base::Value(kNewValue), 0);

  // No value should be set in the account store.
  EXPECT_TRUE(ValueInStoreIsAbsent(*store()->GetAccountPrefStore(),
                                   kNonSyncablePrefName));
  // Value is only set in the local store.
  EXPECT_TRUE(ValueInStoreIs(*store()->GetLocalPrefStore(),
                             kNonSyncablePrefName, kNewValue));
}

class DualLayerUserPrefStoreTestForTypes
    : public DualLayerUserPrefStoreTestBase {
 public:
  DualLayerUserPrefStoreTestForTypes() : DualLayerUserPrefStoreTestBase(true) {}
};

TEST_F(DualLayerUserPrefStoreTestForTypes,
       ShouldAddOnlyEnabledTypePrefsToAccountStore) {
  // Enable only PRIORITY_PREFERENCES
  store()->EnableType(syncer::PRIORITY_PREFERENCES);

  store()->SetValue(kPriorityPrefName, base::Value("priority-value"), 0);
  store()->SetValue(kPrefName, base::Value("pref-value"), 0);

  ASSERT_TRUE(ValueInStoreIs(*store()->GetAccountPrefStore(), kPriorityPrefName,
                             "priority-value"));
  // Regular pref is only added to the local pref store.
  EXPECT_TRUE(ValueInStoreIsAbsent(*store()->GetAccountPrefStore(), kPrefName));
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetLocalPrefStore(), kPrefName, "pref-value"));
}

TEST_F(DualLayerUserPrefStoreTestForTypes,
       ShouldAddPrefsToAccountStoreOnlyAfterEnabled) {
  store()->SetValue(kPrefName, base::Value("pref-value"), 0);

  // Pref is only added to the local pref store.
  EXPECT_TRUE(ValueInStoreIsAbsent(*store()->GetAccountPrefStore(), kPrefName));
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetLocalPrefStore(), kPrefName, "pref-value"));

  store()->EnableType(syncer::PREFERENCES);
  // The pref is not copied to the account store on enable.
  EXPECT_TRUE(ValueInStoreIsAbsent(*store()->GetAccountPrefStore(), kPrefName));

  store()->SetValue(kPrefName, base::Value("new_value"), 0);
  // Both stores are updated now.
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetAccountPrefStore(), kPrefName, "new_value"));
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetLocalPrefStore(), kPrefName, "new_value"));
}

TEST_F(DualLayerUserPrefStoreTestForTypes,
       ShouldClearAllSyncablePrefsOfTypeFromAccountStoreOnDisable) {
  store()->EnableType(syncer::PREFERENCES);
  store()->EnableType(syncer::PRIORITY_PREFERENCES);

  store()->SetValue(kPrefName, base::Value("pref-value"), 0);
  store()->SetValue(kPriorityPrefName, base::Value("priority-value"), 0);

  ASSERT_TRUE(
      ValueInStoreIs(*store()->GetAccountPrefStore(), kPrefName, "pref-value"));
  ASSERT_TRUE(ValueInStoreIs(*store()->GetAccountPrefStore(), kPriorityPrefName,
                             "priority-value"));

  store()->DisableTypeAndClearAccountStore(syncer::PRIORITY_PREFERENCES);
  // The regular pref remains untouched.
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetAccountPrefStore(), kPrefName, "pref-value"));
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetLocalPrefStore(), kPrefName, "pref-value"));

  // Priority prefs are cleared from the account store.
  EXPECT_TRUE(
      ValueInStoreIsAbsent(*store()->GetAccountPrefStore(), kPriorityPrefName));
  // Local pref store is not affected.
  EXPECT_TRUE(ValueInStoreIs(*store()->GetLocalPrefStore(), kPriorityPrefName,
                             "priority-value"));

  // The value should no longer be there in the account store even if the type
  // is enabled again.
  store()->EnableType(syncer::PRIORITY_PREFERENCES);
  EXPECT_TRUE(
      ValueInStoreIsAbsent(*store()->GetAccountPrefStore(), kPriorityPrefName));
  EXPECT_TRUE(ValueInStoreIs(*store()->GetLocalPrefStore(), kPriorityPrefName,
                             "priority-value"));
}

// TODO(crbug.com/1416479): Add tests for pref-merging logic.

}  // namespace sync_preferences
