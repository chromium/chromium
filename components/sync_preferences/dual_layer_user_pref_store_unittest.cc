// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/dual_layer_user_pref_store.h"

#include <map>
#include <set>

#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/testing_pref_store.h"
#include "components/sync_preferences/pref_model_associator_client.h"
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

class TestPrefModelAssociatorClient : public PrefModelAssociatorClient {
 public:
  TestPrefModelAssociatorClient()
      : syncable_prefs_database_(kSyncablePrefsDatabase) {}

  // PrefModelAssociatorClient implementation.
  bool IsMergeableListPreference(const std::string& pref_name) const override {
    return false;
  }

  bool IsMergeableDictionaryPreference(
      const std::string& pref_name) const override {
    return false;
  }

  base::Value MaybeMergePreferenceValues(
      const std::string& pref_name,
      const base::Value& local_value,
      const base::Value& server_value) const override {
    return base::Value();
  }

  const SyncablePrefsDatabase& GetSyncablePrefsDatabase() const override {
    return syncable_prefs_database_;
  }

 private:
  TestSyncablePrefsDatabase syncable_prefs_database_;
};

class DualLayerUserPrefStoreTestBase : public testing::Test {
 public:
  explicit DualLayerUserPrefStoreTestBase(bool initialize) {
    local_store_ = base::MakeRefCounted<TestingPrefStore>();
    dual_layer_store_ = base::MakeRefCounted<DualLayerUserPrefStore>(
        local_store_, &pref_model_associator_client_);

    if (initialize) {
      local_store_->NotifyInitializationCompleted();
    }
  }

  TestingPrefStore* local_store() { return local_store_.get(); }
  DualLayerUserPrefStore* store() { return dual_layer_store_.get(); }

 protected:
  scoped_refptr<TestingPrefStore> local_store_;
  scoped_refptr<DualLayerUserPrefStore> dual_layer_store_;
  TestPrefModelAssociatorClient pref_model_associator_client_;
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

class MergeTestPrefModelAssociatorClient : public PrefModelAssociatorClient {
 public:
  MergeTestPrefModelAssociatorClient()
      : syncable_prefs_database_(kSyncablePrefsDatabase) {}

  // PrefModelAssociatorClient implementation.
  bool IsMergeableListPreference(const std::string& pref_name) const override {
    return mergeable_list_prefs_.count(pref_name);
  }

  bool IsMergeableDictionaryPreference(
      const std::string& pref_name) const override {
    return mergeable_dict_prefs_.count(pref_name);
  }

  base::Value MaybeMergePreferenceValues(
      const std::string& pref_name,
      const base::Value& local_value,
      const base::Value& server_value) const override {
    if (auto it = custom_merge_values_.find(pref_name);
        it != custom_merge_values_.end()) {
      return it->second.Clone();
    }
    return base::Value();
  }

  const SyncablePrefsDatabase& GetSyncablePrefsDatabase() const override {
    return syncable_prefs_database_;
  }

  void MarkAsMergeableDictPref(const std::string& pref_name) {
    mergeable_dict_prefs_.insert(pref_name);
  }

  void MarkAsMergeableListPref(const std::string& pref_name) {
    mergeable_list_prefs_.insert(pref_name);
  }

  void SetCustomMergeValue(const std::string& pref_name, base::Value value) {
    custom_merge_values_[pref_name] = std::move(value);
  }

 private:
  TestSyncablePrefsDatabase syncable_prefs_database_;

  std::set<std::string> mergeable_dict_prefs_;
  std::set<std::string> mergeable_list_prefs_;
  std::map<std::string, base::Value> custom_merge_values_;
};

class DualLayerUserPrefStoreMergeTest : public testing::Test {
 public:
  DualLayerUserPrefStoreMergeTest() {
    local_store_ = base::MakeRefCounted<TestingPrefStore>();
    dual_layer_store_ = base::MakeRefCounted<DualLayerUserPrefStore>(
        local_store_, &pref_model_associator_client_);

    local_store_->NotifyInitializationCompleted();

    dual_layer_store_->AddObserver(&observer_);

    dual_layer_store_->EnableType(syncer::PREFERENCES);
    dual_layer_store_->EnableType(syncer::PRIORITY_PREFERENCES);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    dual_layer_store_->EnableType(syncer::OS_PREFERENCES);
    dual_layer_store_->EnableType(syncer::OS_PRIORITY_PREFERENCES);
#endif
  }

  ~DualLayerUserPrefStoreMergeTest() override {
    dual_layer_store_->RemoveObserver(&observer_);
  }

  DualLayerUserPrefStore* store() { return dual_layer_store_.get(); }

 protected:
  scoped_refptr<TestingPrefStore> local_store_;
  scoped_refptr<DualLayerUserPrefStore> dual_layer_store_;
  MergeTestPrefModelAssociatorClient pref_model_associator_client_;
  testing::StrictMock<MockPrefStoreObserver> observer_;
};

TEST_F(DualLayerUserPrefStoreMergeTest,
       ShouldUseAccountValueForNonMergeablePrefs) {
  // String prefs.
  base::Value account_value("account_value");
  store()->GetAccountPrefStore()->SetValueSilently(kPref1,
                                                   account_value.Clone(), 0);
  base::Value local_value("local_value");
  store()->GetLocalPrefStore()->SetValueSilently(kPref1, local_value.Clone(),
                                                 0);

  // Different values are set in both stores; the one from the account should
  // take precedence.
  // Uses GetValue().
  {
    const base::Value* result = nullptr;
    ASSERT_TRUE(store()->GetValue(kPref1, &result));
    EXPECT_EQ(*result, account_value);
  }
  // Uses GetMutableValue().
  {
    base::Value* result = nullptr;
    ASSERT_TRUE(store()->GetMutableValue(kPref1, &result));
    EXPECT_EQ(*result, account_value);
  }
  // Uses GetValues().
  {
    ASSERT_TRUE(store()->GetValues().contains(kPref1));
    EXPECT_EQ(*store()->GetValues().Find(kPref1), account_value);
  }

  // List prefs.

  base::Value account_list(base::Value::List().Append("account_value"));
  store()->GetAccountPrefStore()->SetValueSilently(kPref2, account_list.Clone(),
                                                   0);
  base::Value local_list(base::Value::List().Append("local_value"));
  store()->GetLocalPrefStore()->SetValueSilently(kPref2, local_list.Clone(), 0);

  // Different values are set in both stores; the one from the account should
  // take precedence.
  // Uses GetValue().
  {
    const base::Value* result = nullptr;
    ASSERT_TRUE(store()->GetValue(kPref2, &result));
    EXPECT_EQ(*result, account_list);
  }
  // Uses GetMutableValue().
  {
    base::Value* result = nullptr;
    ASSERT_TRUE(store()->GetMutableValue(kPref2, &result));
    EXPECT_EQ(*result, account_list);
  }
  // Uses GetValues().
  {
    ASSERT_TRUE(store()->GetValues().contains(kPref2));
    EXPECT_EQ(*store()->GetValues().Find(kPref2), account_list);
  }

  // Dictionary prefs.

  base::Value account_dict(base::Value::Dict()
                               .Set("account_key", "account_value")
                               .Set("common_key", "account_value"));
  store()->GetAccountPrefStore()->SetValueSilently(kPref3, account_dict.Clone(),
                                                   0);
  base::Value local_dict(base::Value::Dict()
                             .Set("local_key", "local_value")
                             .Set("common_key", "local_value"));
  store()->GetLocalPrefStore()->SetValueSilently(kPref3, local_dict.Clone(), 0);

  // Different values are set in both stores; the one from the account should
  // take precedence.
  // Uses GetValue().
  {
    const base::Value* result = nullptr;
    ASSERT_TRUE(store()->GetValue(kPref3, &result));
    EXPECT_EQ(*result, account_dict);
  }
  // Uses GetMutableValue().
  {
    base::Value* result = nullptr;
    ASSERT_TRUE(store()->GetMutableValue(kPref3, &result));
    EXPECT_EQ(*result, account_dict);
  }
  // Uses GetValues().
  {
    ASSERT_TRUE(store()->GetValues().contains(kPref3));
    EXPECT_EQ(*store()->GetValues().Find(kPref3), account_dict);
  }
  // The local and the account stores are left untouched.
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetLocalPrefStore(), kPref1, local_value));
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetAccountPrefStore(), kPref1, account_value));
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetLocalPrefStore(), kPref2, local_list));
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetAccountPrefStore(), kPref2, account_list));
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetLocalPrefStore(), kPref3, local_dict));
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetAccountPrefStore(), kPref3, account_dict));
}

TEST_F(DualLayerUserPrefStoreMergeTest, ShouldMergeMergeableListPref) {
  base::Value account_list(
      base::Value::List().Append("account_value").Append("common_value"));
  store()->GetAccountPrefStore()->SetValueSilently(kPref1, account_list.Clone(),
                                                   0);
  base::Value local_list(
      base::Value::List().Append("local_value").Append("common_value"));
  store()->GetLocalPrefStore()->SetValueSilently(kPref1, local_list.Clone(), 0);

  pref_model_associator_client_.MarkAsMergeableListPref(kPref1);
  // Different values are set in both stores; a merged view should be returned.
  // The two lists should be de-duped, with account values coming first.
  base::Value merged_list(base::Value::List()
                              .Append("account_value")
                              .Append("common_value")
                              .Append("local_value"));

  // Uses GetValue().
  {
    const base::Value* result = nullptr;
    ASSERT_TRUE(store()->GetValue(kPref1, &result));
    EXPECT_EQ(*result, merged_list);
  }
  // Uses GetMutableValue().
  {
    base::Value* result = nullptr;
    ASSERT_TRUE(store()->GetMutableValue(kPref1, &result));
    EXPECT_EQ(*result, merged_list);
  }
  // Uses GetValues().
  {
    ASSERT_TRUE(store()->GetValues().contains(kPref1));
    EXPECT_EQ(*store()->GetValues().Find(kPref1), merged_list);
  }

  // The local and the account stores are left untouched.
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetLocalPrefStore(), kPref1, local_list));
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetAccountPrefStore(), kPref1, account_list));
}

TEST_F(DualLayerUserPrefStoreMergeTest, ShouldMergeMergeableDictPref) {
  base::Value account_dict(base::Value::Dict()
                               .Set("account_key", "account_value")
                               .Set("common_key", "account_value"));
  store()->GetAccountPrefStore()->SetValueSilently(kPref1, account_dict.Clone(),
                                                   0);
  base::Value local_dict(base::Value::Dict()
                             .Set("local_key", "local_value")
                             .Set("common_key", "local_value"));
  store()->GetLocalPrefStore()->SetValueSilently(kPref1, local_dict.Clone(), 0);

  pref_model_associator_client_.MarkAsMergeableDictPref(kPref1);
  // Different values are set in both stores; a merged view should be returned.
  // In case of conflict, the value in account store takes precedence.
  base::Value merged_dict(base::Value::Dict()
                              .Set("account_key", "account_value")
                              .Set("local_key", "local_value")
                              .Set("common_key", "account_value"));
  // Uses GetValue().
  {
    const base::Value* result = nullptr;
    ASSERT_TRUE(store()->GetValue(kPref1, &result));
    EXPECT_EQ(*result, merged_dict);
  }
  // Uses GetMutableValue().
  {
    base::Value* result = nullptr;
    ASSERT_TRUE(store()->GetMutableValue(kPref1, &result));
    EXPECT_EQ(*result, merged_dict);
  }
  // Uses GetValues().
  {
    ASSERT_TRUE(store()->GetValues().contains(kPref1));
    EXPECT_EQ(*store()->GetValues().Find(kPref1), merged_dict);
  }

  // The local and the account stores are left untouched.
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetLocalPrefStore(), kPref1, local_dict));
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetAccountPrefStore(), kPref1, account_dict));
}

TEST_F(DualLayerUserPrefStoreMergeTest, ShouldMergeSpecialCasedMergeablePref) {
  base::Value account_value("account_value");
  store()->GetAccountPrefStore()->SetValueSilently(kPref1,
                                                   account_value.Clone(), 0);
  base::Value local_value("local_value");
  store()->GetLocalPrefStore()->SetValueSilently(kPref1, local_value.Clone(),
                                                 0);

  base::Value merged_value("custom_merge_value");
  pref_model_associator_client_.SetCustomMergeValue(kPref1,
                                                    merged_value.Clone());
  // Different values are set in both stores; the merge should use the custom
  // logic.
  // Uses GetValue().
  {
    const base::Value* result = nullptr;
    ASSERT_TRUE(store()->GetValue(kPref1, &result));
    EXPECT_EQ(*result, merged_value);
  }
  // Uses GetMutableValue().
  {
    base::Value* result = nullptr;
    ASSERT_TRUE(store()->GetMutableValue(kPref1, &result));
    EXPECT_EQ(*result, merged_value);
  }
  // Uses GetValues().
  {
    ASSERT_TRUE(store()->GetValues().contains(kPref1));
    EXPECT_EQ(*store()->GetValues().Find(kPref1), merged_value);
  }

  // The local and the account stores are left untouched.
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetLocalPrefStore(), kPref1, local_value));
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetAccountPrefStore(), kPref1, account_value));
}

TEST_F(DualLayerUserPrefStoreMergeTest,
       ShouldApplyUpdatesToBothStoresForNonMergeablePrefOnSetValue) {
  // Set three prefs; one only in the account store, one only in the local store
  // and one in both stores.
  store()->GetAccountPrefStore()->SetValueSilently(
      kPref1, base::Value("account_value1"), 0);
  store()->GetLocalPrefStore()->SetValueSilently(
      kPref2, base::Value("local_value2"), 0);
  store()->GetAccountPrefStore()->SetValueSilently(
      kPref3, base::Value("account_value3"), 0);
  store()->GetLocalPrefStore()->SetValueSilently(
      kPref3, base::Value("local_value3"), 0);

  // Set an existing account pref. This should not raise a notification, but
  // only writes to the local store.
  store()->SetValue(kPref1, base::Value("account_value1"), 0);
  EXPECT_TRUE(ValueInStoreIs(*store(), kPref1, base::Value("account_value1")));
  EXPECT_TRUE(ValueInStoreIs(*store()->GetLocalPrefStore(), kPref1,
                             base::Value("account_value1")));
  EXPECT_TRUE(ValueInStoreIs(*store()->GetAccountPrefStore(), kPref1,
                             base::Value("account_value1")));

  // Set an existing local pref. This should write to the account store, but not
  // raise a notification.
  store()->SetValue(kPref2, base::Value("local_value2"), 0);
  EXPECT_TRUE(ValueInStoreIs(*store(), kPref2, base::Value("local_value2")));
  EXPECT_TRUE(ValueInStoreIs(*store()->GetLocalPrefStore(), kPref2,
                             base::Value("local_value2")));
  EXPECT_TRUE(ValueInStoreIs(*store()->GetAccountPrefStore(), kPref2,
                             base::Value("local_value2")));

  // Update the common pref. This writes to both stores and raises notification
  // because the effective value changed.
  EXPECT_CALL(observer_, OnPrefValueChanged(kPref3));
  store()->SetValue(kPref3, base::Value("new_value3"), 0);
  EXPECT_TRUE(ValueInStoreIs(*store(), kPref3, base::Value("new_value3")));
  EXPECT_TRUE(ValueInStoreIs(*store()->GetLocalPrefStore(), kPref3,
                             base::Value("new_value3")));
  EXPECT_TRUE(ValueInStoreIs(*store()->GetAccountPrefStore(), kPref3,
                             base::Value("new_value3")));
}

TEST_F(DualLayerUserPrefStoreMergeTest,
       ShouldApplyUpdatesToBothStoresForNonMergeablePrefOnSetValueSilently) {
  // Set three prefs; one only in the account store, one only in the local store
  // and one in both stores.
  store()->GetAccountPrefStore()->SetValueSilently(
      kPref1, base::Value("account_value1"), 0);
  store()->GetLocalPrefStore()->SetValueSilently(
      kPref2, base::Value("local_value2"), 0);
  store()->GetAccountPrefStore()->SetValueSilently(
      kPref3, base::Value("account_value3"), 0);
  store()->GetLocalPrefStore()->SetValueSilently(
      kPref3, base::Value("local_value3"), 0);

  // Set an existing account pref. This should write to the local store.
  store()->SetValueSilently(kPref1, base::Value("account_value1"), 0);
  EXPECT_TRUE(ValueInStoreIs(*store(), kPref1, base::Value("account_value1")));
  EXPECT_TRUE(ValueInStoreIs(*store()->GetLocalPrefStore(), kPref1,
                             base::Value("account_value1")));
  EXPECT_TRUE(ValueInStoreIs(*store()->GetAccountPrefStore(), kPref1,
                             base::Value("account_value1")));

  // Set an existing local pref. This should write to the account store.
  store()->SetValueSilently(kPref2, base::Value("local_value2"), 0);
  EXPECT_TRUE(ValueInStoreIs(*store(), kPref2, base::Value("local_value2")));
  EXPECT_TRUE(ValueInStoreIs(*store()->GetLocalPrefStore(), kPref2,
                             base::Value("local_value2")));
  EXPECT_TRUE(ValueInStoreIs(*store()->GetAccountPrefStore(), kPref2,
                             base::Value("local_value2")));

  // Update the common pref. This writes to both stores.
  store()->SetValueSilently(kPref3, base::Value("new_value3"), 0);
  EXPECT_TRUE(ValueInStoreIs(*store(), kPref3, base::Value("new_value3")));
  EXPECT_TRUE(ValueInStoreIs(*store()->GetLocalPrefStore(), kPref3,
                             base::Value("new_value3")));
  EXPECT_TRUE(ValueInStoreIs(*store()->GetAccountPrefStore(), kPref3,
                             base::Value("new_value3")));
}

TEST_F(DualLayerUserPrefStoreMergeTest,
       ShouldApplyUpdatesToBothStoresForNonMergeablePrefOnReportValueChanged) {
  // Set three prefs; one only in the account store, one only in the local store
  // and one in both stores.
  store()->GetAccountPrefStore()->SetValueSilently(
      kPref1, base::Value("account_value1"), 0);
  store()->GetLocalPrefStore()->SetValueSilently(
      kPref2, base::Value("local_value2"), 0);
  store()->GetAccountPrefStore()->SetValueSilently(
      kPref3, base::Value("account_value3"), 0);
  store()->GetLocalPrefStore()->SetValueSilently(
      kPref3, base::Value("local_value3"), 0);

  // Set an existing account pref. This writes to both stores and raises
  // notification.
  EXPECT_CALL(observer_, OnPrefValueChanged(kPref1));
  store()->ReportValueChanged(kPref1, 0);
  EXPECT_TRUE(ValueInStoreIs(*store(), kPref1, base::Value("account_value1")));
  EXPECT_TRUE(ValueInStoreIs(*store()->GetLocalPrefStore(), kPref1,
                             base::Value("account_value1")));
  EXPECT_TRUE(ValueInStoreIs(*store()->GetAccountPrefStore(), kPref1,
                             base::Value("account_value1")));

  // Set an existing local pref. This writes to both stores and raises
  // notification.
  EXPECT_CALL(observer_, OnPrefValueChanged(kPref2));
  store()->ReportValueChanged(kPref2, 0);
  EXPECT_TRUE(ValueInStoreIs(*store(), kPref2, base::Value("local_value2")));
  EXPECT_TRUE(ValueInStoreIs(*store()->GetLocalPrefStore(), kPref2,
                             base::Value("local_value2")));
  EXPECT_TRUE(ValueInStoreIs(*store()->GetAccountPrefStore(), kPref2,
                             base::Value("local_value2")));

  // Update the common pref. This writes to both stores and raises notification.
  EXPECT_CALL(observer_, OnPrefValueChanged(kPref3));
  store()->ReportValueChanged(kPref3, 0);
  EXPECT_TRUE(ValueInStoreIs(*store(), kPref3, base::Value("account_value3")));
  EXPECT_TRUE(ValueInStoreIs(*store()->GetLocalPrefStore(), kPref3,
                             base::Value("account_value3")));
  EXPECT_TRUE(ValueInStoreIs(*store()->GetAccountPrefStore(), kPref3,
                             base::Value("account_value3")));
}

TEST_F(DualLayerUserPrefStoreMergeTest,
       ShouldUpdateMergedPrefOnWriteToUnderlyingStoresUsingSetValue) {
  pref_model_associator_client_.MarkAsMergeableDictPref(kPref1);

  store()->GetAccountPrefStore()->SetValueSilently(
      kPref1,
      base::Value(base::Value::Dict()
                      .Set("account_key", "account_value")
                      .Set("common_key", "account_value")),
      0);
  store()->GetLocalPrefStore()->SetValueSilently(
      kPref1,
      base::Value(base::Value::Dict()
                      .Set("local_key", "local_value")
                      .Set("common_key", "local_value")),
      0);

  ASSERT_TRUE(
      ValueInStoreIs(*store(), kPref1,
                     base::Value(base::Value::Dict()
                                     .Set("account_key", "account_value")
                                     .Set("local_key", "local_value")
                                     .Set("common_key", "account_value"))));

  EXPECT_CALL(observer_, OnPrefValueChanged(kPref1));
  // Update account value.
  store()->GetAccountPrefStore()->SetValue(
      kPref1,
      base::Value(base::Value::Dict()
                      .Set("account_key", "new_account_value")
                      .Set("common_key", "account_value")),
      0);

  // Updated account value should reflect in the merged view.
  EXPECT_TRUE(
      ValueInStoreIs(*store(), kPref1,
                     base::Value(base::Value::Dict()
                                     // Updated value.
                                     .Set("account_key", "new_account_value")
                                     .Set("local_key", "local_value")
                                     .Set("common_key", "account_value"))));

  EXPECT_CALL(observer_, OnPrefValueChanged(kPref1));
  // Add new key to local value.
  store()->GetLocalPrefStore()->SetValue(
      kPref1,
      base::Value(base::Value::Dict()
                      .Set("local_key", "local_value")
                      // New entry.
                      .Set("new_local_key", "local_value")
                      .Set("common_key", "local_value")),
      0);

  // Updated local value should reflect in the merged view.
  EXPECT_TRUE(
      ValueInStoreIs(*store(), kPref1,
                     base::Value(base::Value::Dict()
                                     .Set("account_key", "new_account_value")
                                     .Set("local_key", "local_value")
                                     // New entry.
                                     .Set("new_local_key", "local_value")
                                     .Set("common_key", "account_value"))));
}

TEST_F(DualLayerUserPrefStoreMergeTest,
       ShouldUpdateMergedPrefOnWriteToUnderlyingStoresUsingSetValueSilently) {
  pref_model_associator_client_.MarkAsMergeableDictPref(kPref1);

  store()->GetAccountPrefStore()->SetValueSilently(
      kPref1,
      base::Value(base::Value::Dict()
                      .Set("account_key", "account_value")
                      .Set("common_key", "account_value")),
      0);
  store()->GetLocalPrefStore()->SetValueSilently(
      kPref1,
      base::Value(base::Value::Dict()
                      .Set("local_key", "local_value")
                      .Set("common_key", "local_value")),
      0);

  ASSERT_TRUE(
      ValueInStoreIs(*store(), kPref1,
                     base::Value(base::Value::Dict()
                                     .Set("account_key", "account_value")
                                     .Set("local_key", "local_value")
                                     .Set("common_key", "account_value"))));

  // Update account value.
  store()->GetAccountPrefStore()->SetValueSilently(
      kPref1,
      base::Value(base::Value::Dict()
                      // Updated value.
                      .Set("account_key", "new_account_value")
                      .Set("common_key", "account_value")),
      0);

  // Updated account value should reflect in the merged view.
  EXPECT_TRUE(
      ValueInStoreIs(*store(), kPref1,
                     base::Value(base::Value::Dict()
                                     // Updated value.
                                     .Set("account_key", "new_account_value")
                                     .Set("local_key", "local_value")
                                     .Set("common_key", "account_value"))));

  // Add new key to local value.
  store()->GetLocalPrefStore()->SetValueSilently(
      kPref1,
      base::Value(base::Value::Dict()
                      .Set("local_key", "local_value")
                      .Set("new_local_key", "local_value")
                      .Set("common_key", "local_value")),
      0);

  // Updated local value should reflect in the merged view.
  EXPECT_TRUE(
      ValueInStoreIs(*store(), kPref1,
                     base::Value(base::Value::Dict()
                                     .Set("account_key", "new_account_value")
                                     .Set("local_key", "local_value")
                                     // New entry.
                                     .Set("new_local_key", "local_value")
                                     .Set("common_key", "account_value"))));
}

TEST_F(DualLayerUserPrefStoreMergeTest,
       ShouldUpdateMergedPrefOnWriteToUnderlyingStoresUsingMutableValue) {
  pref_model_associator_client_.MarkAsMergeableDictPref(kPref1);

  store()->GetAccountPrefStore()->SetValueSilently(
      kPref1,
      base::Value(base::Value::Dict()
                      .Set("account_key", "account_value")
                      .Set("common_key", "account_value")),
      0);
  store()->GetLocalPrefStore()->SetValueSilently(
      kPref1,
      base::Value(base::Value::Dict()
                      .Set("local_key", "local_value")
                      .Set("common_key", "local_value")),
      0);

  ASSERT_TRUE(
      ValueInStoreIs(*store(), kPref1,
                     base::Value(base::Value::Dict()
                                     .Set("account_key", "account_value")
                                     .Set("local_key", "local_value")
                                     .Set("common_key", "account_value"))));

  base::Value* account_value = nullptr;
  store()->GetAccountPrefStore()->GetMutableValue(kPref1, &account_value);
  ASSERT_TRUE(account_value && account_value->is_dict());

  // Update account value.
  *account_value = base::Value(base::Value::Dict()
                                   // Updated value.
                                   .Set("account_key", "new_account_value")
                                   .Set("common_key", "account_value"));

  EXPECT_CALL(observer_, OnPrefValueChanged(kPref1));
  store()->GetAccountPrefStore()->ReportValueChanged(kPref1, 0);

  // Updated account value should reflect in the merged view.
  EXPECT_TRUE(
      ValueInStoreIs(*store(), kPref1,
                     base::Value(base::Value::Dict()
                                     // Updated value.
                                     .Set("account_key", "new_account_value")
                                     .Set("local_key", "local_value")
                                     .Set("common_key", "account_value"))));

  base::Value* local_value = nullptr;
  store()->GetLocalPrefStore()->GetMutableValue(kPref1, &local_value);
  ASSERT_TRUE(local_value && local_value->is_dict());
  // Add new key to local value.
  local_value->GetDict().Set("new_local_key", "local_value");

  EXPECT_CALL(observer_, OnPrefValueChanged(kPref1));
  store()->GetLocalPrefStore()->ReportValueChanged(kPref1, 0);

  // Updated local value should reflect in the merged view.
  EXPECT_TRUE(
      ValueInStoreIs(*store(), kPref1,
                     base::Value(base::Value::Dict()
                                     .Set("account_key", "new_account_value")
                                     .Set("local_key", "local_value")
                                     // New entry.
                                     .Set("new_local_key", "local_value")
                                     .Set("common_key", "account_value"))));
}

TEST_F(DualLayerUserPrefStoreMergeTest,
       ShouldUpdateMergedPrefOnRemoveFromUnderlyingStores) {
  pref_model_associator_client_.MarkAsMergeableDictPref(kPref1);

  store()->GetAccountPrefStore()->SetValueSilently(
      kPref1,
      base::Value(base::Value::Dict()
                      .Set("account_key", "account_value")
                      .Set("common_key", "account_value")),
      0);
  store()->GetLocalPrefStore()->SetValueSilently(
      kPref1,
      base::Value(base::Value::Dict()
                      .Set("local_key", "local_value")
                      .Set("common_key", "local_value")),
      0);

  ASSERT_TRUE(
      ValueInStoreIs(*store(), kPref1,
                     base::Value(base::Value::Dict()
                                     .Set("account_key", "account_value")
                                     .Set("local_key", "local_value")
                                     .Set("common_key", "account_value"))));

  // Remove pref from the account store.
  store()->GetAccountPrefStore()->RemoveValuesByPrefixSilently(kPref1);
  EXPECT_TRUE(
      ValueInStoreIs(*store(), kPref1,
                     base::Value(base::Value::Dict()
                                     .Set("local_key", "local_value")
                                     // Value now being by the local store.
                                     .Set("common_key", "local_value"))));

  // Remove pref from the local store.
  EXPECT_CALL(observer_, OnPrefValueChanged(kPref1));
  store()->GetLocalPrefStore()->RemoveValue(kPref1, 0);
  EXPECT_TRUE(ValueInStoreIsAbsent(*store(), kPref1));
}

TEST_F(DualLayerUserPrefStoreMergeTest, ShouldClearMergedPrefOnRemove) {
  // Ensures that pref no longer exists in the merged pref store upon remove.
  store()->GetAccountPrefStore()->SetValueSilently(
      kPref1,
      base::Value(base::Value::Dict()
                      .Set("account_key", "account_value")
                      .Set("common_key", "account_value")),
      0);
  store()->GetLocalPrefStore()->SetValueSilently(
      kPref1,
      base::Value(base::Value::Dict()
                      .Set("local_key", "local_value")
                      .Set("common_key", "local_value")),
      0);

  pref_model_associator_client_.MarkAsMergeableDictPref(kPref1);
  ASSERT_TRUE(
      ValueInStoreIs(*store(), kPref1,
                     base::Value(base::Value::Dict()
                                     .Set("account_key", "account_value")
                                     .Set("local_key", "local_value")
                                     .Set("common_key", "account_value"))));

  EXPECT_CALL(observer_, OnPrefValueChanged(kPref1));
  store()->RemoveValue(kPref1, 0);
  EXPECT_TRUE(ValueInStoreIsAbsent(*store(), kPref1));

  pref_model_associator_client_.MarkAsMergeableDictPref(kPref2);
  store()->GetAccountPrefStore()->SetValueSilently(
      kPref2,
      base::Value(base::Value::Dict()
                      .Set("account_key", "account_value")
                      .Set("common_key", "account_value")),
      0);
  store()->GetLocalPrefStore()->SetValueSilently(
      kPref2,
      base::Value(base::Value::Dict()
                      .Set("local_key", "local_value")
                      .Set("common_key", "local_value")),
      0);

  ASSERT_TRUE(
      ValueInStoreIs(*store(), kPref2,
                     base::Value(base::Value::Dict()
                                     .Set("account_key", "account_value")
                                     .Set("local_key", "local_value")
                                     .Set("common_key", "account_value"))));

  store()->RemoveValuesByPrefixSilently(kPref2);
  EXPECT_TRUE(ValueInStoreIsAbsent(*store(), kPref2));
}

TEST_F(DualLayerUserPrefStoreMergeTest,
       ShouldUnmergeMergeableDictPrefButNotAddUnchangedValueToAccountStore) {
  pref_model_associator_client_.MarkAsMergeableDictPref(kPref1);

  base::Value local_dict(base::Value::Dict().Set("local_key", "local_value"));
  store()->GetLocalPrefStore()->SetValueSilently(kPref1, local_dict.Clone(), 0);

  // `kPref1` only exists in the local store.
  ASSERT_TRUE(ValueInStoreIsAbsent(*store()->GetAccountPrefStore(), kPref1));

  // Effective value same as local value since pref is not in account store.
  ASSERT_TRUE(ValueInStoreIs(*store(), kPref1, local_dict));

  // Set the effective/merged value again.
  // Note: Expecting no notification.
  store()->SetValue(kPref1, local_dict.Clone(), 0);

  // Value in the local store remains unchanged.
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetLocalPrefStore(), kPref1, local_dict));
  // An empty dict pref is added to the account store.
  // Note: This is an implementation detail. Ideally, not adding the pref to the
  // account store might be a better approach.
  EXPECT_TRUE(ValueInStoreIs(*store()->GetAccountPrefStore(), kPref1,
                             base::Value(base::Value::Type::DICT)));
}

TEST_F(DualLayerUserPrefStoreMergeTest,
       ShouldUnmergeMergeableDictPrefButNotAddUnchangedValueToLocalStore) {
  pref_model_associator_client_.MarkAsMergeableDictPref(kPref1);

  base::Value account_dict(
      base::Value::Dict().Set("account_key", "account_value"));
  store()->GetAccountPrefStore()->SetValueSilently(kPref1, account_dict.Clone(),
                                                   0);
  // `kPref1` only exists in the account store.
  ASSERT_TRUE(ValueInStoreIsAbsent(*store()->GetLocalPrefStore(), kPref1));

  ASSERT_TRUE(ValueInStoreIs(*store(), kPref1, account_dict));

  // Set the effective/merged value again.
  // Note: Expecting no notification.
  store()->SetValue(kPref1, account_dict.Clone(), 0);

  // An empty dict pref is added to the local store.
  // Note: This is an implementation detail. Ideally, not adding the pref to the
  // local store might be a better approach.
  EXPECT_TRUE(ValueInStoreIs(*store()->GetLocalPrefStore(), kPref1,
                             base::Value(base::Value::Type::DICT)));
  // Value in the account store remains unchanged.
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetAccountPrefStore(), kPref1, account_dict));
}

TEST_F(DualLayerUserPrefStoreMergeTest,
       ShouldUnmergeAndApplyUpdatesForMergeableDictPrefOnSetValue) {
  pref_model_associator_client_.MarkAsMergeableDictPref(kPref1);

  base::Value local_dict(base::Value::Dict()
                             .Set("local_key1", "local_value1")
                             .Set("local_key2", "local_value2")
                             .Set("local_key3", "local_value3")
                             .Set("common_key1", "local_value4")
                             .Set("common_key2", "common_value"));
  store()->GetLocalPrefStore()->SetValueSilently(kPref1, local_dict.Clone(), 0);

  base::Value account_dict(base::Value::Dict()
                               .Set("account_key1", "account_value1")
                               .Set("account_key2", "account_value2")
                               .Set("account_key3", "account_value3")
                               .Set("common_key1", "account_value4")
                               .Set("common_key2", "common_value"));
  store()->GetAccountPrefStore()->SetValueSilently(kPref1, account_dict.Clone(),
                                                   0);

  base::Value merged_dict(base::Value::Dict()
                              .Set("account_key1", "account_value1")
                              .Set("account_key2", "account_value2")
                              .Set("account_key3", "account_value3")
                              .Set("common_key1", "account_value4")
                              .Set("common_key2", "common_value")
                              .Set("local_key1", "local_value1")
                              .Set("local_key2", "local_value2")
                              .Set("local_key3", "local_value3"));
  ASSERT_TRUE(ValueInStoreIs(*store(), kPref1, merged_dict));

  base::Value updated_dict(base::Value::Dict()
                               // New key, should get added to both
                               // stores.
                               .Set("new_key", "new_value")
                               // Updated value, should get added to both
                               // stores.
                               .Set("account_key1", "new_value1")
                               // No change, should only be in account
                               // store.
                               .Set("account_key2", "account_value2")
                               // No change, should only be in local
                               // store.
                               .Set("local_key1", "local_value1")
                               // Updated value, should get added to both
                               // stores.
                               .Set("local_key2", "new_value2")
                               // Updated value, should get added to both
                               // stores.
                               .Set("common_key1", "local_value4")
                               // Updated value, should get added to both
                               // stores.
                               .Set("common_key2", "new_common_value"));
  EXPECT_CALL(observer_, OnPrefValueChanged(kPref1));
  store()->SetValue(kPref1, updated_dict.Clone(), 0);

  // Note: "local_key3" has been deleted.
  base::Value updated_local_dict(base::Value::Dict()
                                     .Set("new_key", "new_value")
                                     .Set("account_key1", "new_value1")
                                     .Set("local_key1", "local_value1")
                                     .Set("local_key2", "new_value2")
                                     .Set("common_key1", "local_value4")
                                     .Set("common_key2", "new_common_value"));
  EXPECT_TRUE(ValueInStoreIs(*store()->GetLocalPrefStore(), kPref1,
                             updated_local_dict));

  // Note: "account_key3" has been deleted.
  base::Value updated_account_dict(base::Value::Dict()
                                       .Set("new_key", "new_value")
                                       .Set("account_key1", "new_value1")
                                       .Set("account_key2", "account_value2")
                                       .Set("local_key2", "new_value2")
                                       .Set("common_key1", "local_value4")
                                       .Set("common_key2", "new_common_value"));
  EXPECT_TRUE(ValueInStoreIs(*store()->GetAccountPrefStore(), kPref1,
                             updated_account_dict));

  ASSERT_TRUE(ValueInStoreIs(*store(), kPref1, updated_dict));
}

TEST_F(DualLayerUserPrefStoreMergeTest,
       ShouldUnmergeAndApplyUpdatesForMergeableDictPrefOnSetValueSilently) {
  pref_model_associator_client_.MarkAsMergeableDictPref(kPref1);

  base::Value local_dict(base::Value::Dict()
                             .Set("local_key1", "local_value1")
                             .Set("local_key2", "local_value2")
                             .Set("local_key3", "local_value3")
                             .Set("common_key1", "local_value4")
                             .Set("common_key2", "common_value"));
  store()->GetLocalPrefStore()->SetValueSilently(kPref1, local_dict.Clone(), 0);

  base::Value account_dict(base::Value::Dict()
                               .Set("account_key1", "account_value1")
                               .Set("account_key2", "account_value2")
                               .Set("account_key3", "account_value3")
                               .Set("common_key1", "account_value4")
                               .Set("common_key2", "common_value"));
  store()->GetAccountPrefStore()->SetValueSilently(kPref1, account_dict.Clone(),
                                                   0);

  base::Value merged_dict(base::Value::Dict()
                              .Set("account_key1", "account_value1")
                              .Set("account_key2", "account_value2")
                              .Set("account_key3", "account_value3")
                              .Set("common_key1", "account_value4")
                              .Set("common_key2", "common_value")
                              .Set("local_key1", "local_value1")
                              .Set("local_key2", "local_value2")
                              .Set("local_key3", "local_value3"));
  ASSERT_TRUE(ValueInStoreIs(*store(), kPref1, merged_dict));

  base::Value updated_dict(base::Value::Dict()
                               // New key, should get added to both
                               // stores.
                               .Set("new_key", "new_value")
                               // Updated value, should get added to both
                               // stores.
                               .Set("account_key1", "new_value1")
                               // No change, should only be in account
                               // store.
                               .Set("account_key2", "account_value2")
                               // No change, should only be in local
                               // store.
                               .Set("local_key1", "local_value1")
                               // Updated value, should get added to both
                               // stores.
                               .Set("local_key2", "new_value2")
                               // Updated value, should get added to both
                               // stores.
                               .Set("common_key1", "local_value4")
                               // Updated value, should get added to both
                               // stores.
                               .Set("common_key2", "new_common_value"));
  store()->SetValueSilently(kPref1, updated_dict.Clone(), 0);

  // Note: "local_key3" has been deleted.
  base::Value updated_local_dict(base::Value::Dict()
                                     .Set("new_key", "new_value")
                                     .Set("account_key1", "new_value1")
                                     .Set("local_key1", "local_value1")
                                     .Set("local_key2", "new_value2")
                                     .Set("common_key1", "local_value4")
                                     .Set("common_key2", "new_common_value"));
  EXPECT_TRUE(ValueInStoreIs(*store()->GetLocalPrefStore(), kPref1,
                             updated_local_dict));

  // Note: "account_key3" has been deleted.
  base::Value updated_account_dict(base::Value::Dict()
                                       .Set("new_key", "new_value")
                                       .Set("account_key1", "new_value1")
                                       .Set("account_key2", "account_value2")
                                       .Set("local_key2", "new_value2")
                                       .Set("common_key1", "local_value4")
                                       .Set("common_key2", "new_common_value"));
  EXPECT_TRUE(ValueInStoreIs(*store()->GetAccountPrefStore(), kPref1,
                             updated_account_dict));

  ASSERT_TRUE(ValueInStoreIs(*store(), kPref1, updated_dict));
}

TEST_F(DualLayerUserPrefStoreMergeTest,
       ShouldUnmergeAndApplyUpdatesForMergeableDictPrefOnReportPrefChanged) {
  pref_model_associator_client_.MarkAsMergeableDictPref(kPref1);

  base::Value local_dict(base::Value::Dict()
                             .Set("local_key1", "local_value1")
                             .Set("local_key2", "local_value2")
                             .Set("local_key3", "local_value3")
                             .Set("common_key1", "local_value4")
                             .Set("common_key2", "common_value"));
  store()->GetLocalPrefStore()->SetValueSilently(kPref1, local_dict.Clone(), 0);

  base::Value account_dict(base::Value::Dict()
                               .Set("account_key1", "account_value1")
                               .Set("account_key2", "account_value2")
                               .Set("account_key3", "account_value3")
                               .Set("common_key1", "account_value4")
                               .Set("common_key2", "common_value"));
  store()->GetAccountPrefStore()->SetValueSilently(kPref1, account_dict.Clone(),
                                                   0);

  base::Value merged_dict(base::Value::Dict()
                              .Set("account_key1", "account_value1")
                              .Set("account_key2", "account_value2")
                              .Set("account_key3", "account_value3")
                              .Set("common_key1", "account_value4")
                              .Set("common_key2", "common_value")
                              .Set("local_key1", "local_value1")
                              .Set("local_key2", "local_value2")
                              .Set("local_key3", "local_value3"));
  base::Value* merged_value = nullptr;
  ASSERT_TRUE(store()->GetMutableValue(kPref1, &merged_value));
  ASSERT_EQ(*merged_value, merged_dict);

  base::Value updated_dict(base::Value::Dict()
                               // New key, should get added to both
                               // stores.
                               .Set("new_key", "new_value")
                               // Updated value, should get added to both
                               // stores.
                               .Set("account_key1", "new_value1")
                               // No change, should only be in account
                               // store.
                               .Set("account_key2", "account_value2")
                               // No change, should only be in local
                               // store.
                               .Set("local_key1", "local_value1")
                               // Updated value, should get added to both
                               // stores.
                               .Set("local_key2", "new_value2")
                               // Updated value, should get added to both
                               // stores.
                               .Set("common_key1", "local_value4")
                               // Updated value, should get added to both
                               // stores.
                               .Set("common_key2", "new_common_value"));
  *merged_value = updated_dict.Clone();
  EXPECT_CALL(observer_, OnPrefValueChanged(kPref1));
  store()->ReportValueChanged(kPref1, 0);

  // Note: "local_key3" has been deleted.
  base::Value updated_local_dict(base::Value::Dict()
                                     .Set("new_key", "new_value")
                                     .Set("account_key1", "new_value1")
                                     .Set("local_key1", "local_value1")
                                     .Set("local_key2", "new_value2")
                                     .Set("common_key1", "local_value4")
                                     .Set("common_key2", "new_common_value"));
  EXPECT_TRUE(ValueInStoreIs(*store()->GetLocalPrefStore(), kPref1,
                             updated_local_dict));

  // Note: "account_key3" has been deleted.
  base::Value updated_account_dict(base::Value::Dict()
                                       .Set("new_key", "new_value")
                                       .Set("account_key1", "new_value1")
                                       .Set("account_key2", "account_value2")
                                       .Set("local_key2", "new_value2")
                                       .Set("common_key1", "local_value4")
                                       .Set("common_key2", "new_common_value"));
  EXPECT_TRUE(ValueInStoreIs(*store()->GetAccountPrefStore(), kPref1,
                             updated_account_dict));

  ASSERT_TRUE(ValueInStoreIs(*store(), kPref1, updated_dict));
}

TEST_F(DualLayerUserPrefStoreMergeTest,
       ShouldApplyUpdateOnMergeableListPrefAsNonMergeablePref) {
  pref_model_associator_client_.MarkAsMergeableListPref(kPref1);

  base::Value local_list(
      base::Value::List().Append("local_value").Append("common_value"));
  store()->GetLocalPrefStore()->SetValueSilently(kPref1, local_list.Clone(), 0);

  base::Value account_list(
      base::Value::List().Append("account_value").Append("common_value"));
  store()->GetAccountPrefStore()->SetValueSilently(kPref1, account_list.Clone(),
                                                   0);

  base::Value merged_list(base::Value::List()
                              .Append("account_value")
                              .Append("common_value")
                              .Append("local_value"));
  ASSERT_TRUE(ValueInStoreIs(*store(), kPref1, merged_list));

  base::Value updated_list(base::Value::List()
                               .Append("local_value")
                               .Append("account_value")
                               .Append("common_value"));

  EXPECT_CALL(observer_, OnPrefValueChanged(kPref1));
  // Writes to both stores.
  store()->SetValue(kPref1, updated_list.Clone(), 0);

  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetLocalPrefStore(), kPref1, updated_list));
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetAccountPrefStore(), kPref1, updated_list));

  ASSERT_TRUE(ValueInStoreIs(*store(), kPref1, updated_list));
}

TEST_F(DualLayerUserPrefStoreMergeTest,
       ShouldNotUnmergeIfIncorrectlyMarkedAsMergeableDict) {
  pref_model_associator_client_.MarkAsMergeableDictPref(kPref1);

  base::Value local_dict_value(
      base::Value::Dict().Set("local_key", "local_value"));
  store()->GetLocalPrefStore()->SetValueSilently(kPref1,
                                                 local_dict_value.Clone(), 0);

  base::Value account_dict_value(
      base::Value::Dict().Set("account_key", "account_value"));
  store()->GetAccountPrefStore()->SetValueSilently(
      kPref1, account_dict_value.Clone(), 0);

  base::Value new_value("new_value");

  EXPECT_CALL(observer_, OnPrefValueChanged(kPref1));
  store()->SetValue(kPref1, new_value.Clone(), 0);

  ASSERT_TRUE(ValueInStoreIs(*store(), kPref1, new_value));
  // `kPref1` is considered as incorrectly marked as mergeable and is treated as
  // a scalar value and overwritten to both stores.
  EXPECT_TRUE(ValueInStoreIs(*store()->GetLocalPrefStore(), kPref1, new_value));
  EXPECT_TRUE(
      ValueInStoreIs(*store()->GetAccountPrefStore(), kPref1, new_value));
}

}  // namespace
}  // namespace sync_preferences
