// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/pref_service_syncable.h"

#include <stdint.h>

#include <memory>

#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_store.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model/sync_error_factory_mock.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync/protocol/preference_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync_preferences/pref_model_associator.h"
#include "components/sync_preferences/pref_model_associator_client.h"
#include "components/sync_preferences/synced_pref_observer.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::SyncChange;
using syncer::SyncData;
using testing::Eq;
using testing::IsEmpty;
using testing::Not;
using testing::NotNull;
using testing::SizeIs;

namespace sync_preferences {

namespace {

const char kExampleUrl0[] = "http://example.com/0";
const char kExampleUrl1[] = "http://example.com/1";
const char kExampleUrl2[] = "http://example.com/2";
const char kStringPrefName[] = "string_pref_name";
const char kListPrefName[] = "list_pref_name";
const char kDictPrefName[] = "dict_pref_name";
const char kUnsyncedPreferenceName[] = "nonsense_pref_name";
const char kUnsyncedPreferenceDefaultValue[] = "default";
const char kDefaultCharsetPrefName[] = "default_charset";
const char kNonDefaultCharsetValue[] = "foo";
const char kDefaultCharsetValue[] = "utf-8";

void Increment(int* num) {
  (*num)++;
}

class TestSyncProcessorStub : public syncer::SyncChangeProcessor {
 public:
  explicit TestSyncProcessorStub(syncer::SyncChangeList* output)
      : output_(output), fail_next_(false) {}

  syncer::SyncError ProcessSyncChanges(
      const base::Location& from_here,
      const syncer::SyncChangeList& change_list) override {
    if (output_)
      output_->insert(output_->end(), change_list.begin(), change_list.end());
    if (fail_next_) {
      fail_next_ = false;
      return syncer::SyncError(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                               "Error", syncer::PREFERENCES);
    }
    return syncer::SyncError();
  }

  void FailNextProcessSyncChanges() { fail_next_ = true; }

  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override {
    return syncer::SyncDataList();
  }

 private:
  syncer::SyncChangeList* output_;
  bool fail_next_;
};

class PrefServiceSyncableTest : public testing::Test {
 public:
  PrefServiceSyncableTest()
      : pref_sync_service_(nullptr),
        next_pref_remote_sync_node_id_(0) {}

  void SetUp() override {
    prefs_.registry()->RegisterStringPref(kUnsyncedPreferenceName,
                                          kUnsyncedPreferenceDefaultValue);
    prefs_.registry()->RegisterStringPref(
        kStringPrefName, std::string(),
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
    prefs_.registry()->RegisterListPref(
        kListPrefName, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
    prefs_.registry()->RegisterStringPref(
        kDefaultCharsetPrefName, kDefaultCharsetValue,
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

    pref_sync_service_ = static_cast<PrefModelAssociator*>(
        prefs_.GetSyncableService(syncer::PREFERENCES));
    ASSERT_TRUE(pref_sync_service_);
  }

  syncer::SyncChange MakeRemoteChange(int64_t id,
                                      const std::string& name,
                                      const base::Value& value,
                                      SyncChange::SyncChangeType type) {
    std::string serialized;
    JSONStringValueSerializer json(&serialized);
    if (!json.Serialize(value))
      return syncer::SyncChange();
    sync_pb::EntitySpecifics entity;
    sync_pb::PreferenceSpecifics* pref_one = entity.mutable_preference();
    pref_one->set_name(name);
    pref_one->set_value(serialized);
    return syncer::SyncChange(
        FROM_HERE, type,
        syncer::SyncData::CreateRemoteData(id, entity, base::Time()));
  }

  void AddToRemoteDataList(const std::string& name,
                           const base::Value& value,
                           syncer::SyncDataList* out) {
    std::string serialized;
    JSONStringValueSerializer json(&serialized);
    ASSERT_TRUE(json.Serialize(value));
    sync_pb::EntitySpecifics one;
    sync_pb::PreferenceSpecifics* pref_one = one.mutable_preference();
    pref_one->set_name(name);
    pref_one->set_value(serialized);
    out->push_back(SyncData::CreateRemoteData(++next_pref_remote_sync_node_id_,
                                              one, base::Time()));
  }

  void InitWithSyncDataTakeOutput(const syncer::SyncDataList& initial_data,
                                  syncer::SyncChangeList* output) {
    syncer::SyncMergeResult r = pref_sync_service_->MergeDataAndStartSyncing(
        syncer::PREFERENCES, initial_data,
        std::make_unique<TestSyncProcessorStub>(output),
        std::make_unique<syncer::SyncErrorFactoryMock>());
    EXPECT_FALSE(r.error().IsSet());
  }

  void InitWithNoSyncData() {
    InitWithSyncDataTakeOutput(syncer::SyncDataList(), nullptr);
  }

  const base::Value& GetPreferenceValue(const std::string& name) {
    const PrefService::Preference* preference =
        prefs_.FindPreference(name.c_str());
    return *preference->GetValue();
  }

  std::unique_ptr<base::Value> FindValue(const std::string& name,
                                         const syncer::SyncChangeList& list) {
    auto it = list.begin();
    for (; it != list.end(); ++it) {
      if (syncer::SyncDataLocal(it->sync_data()).GetTag() == name) {
        return base::JSONReader::Read(
            it->sync_data().GetSpecifics().preference().value());
      }
    }
    return nullptr;
  }

  bool IsSynced(const std::string& pref_name) {
    return pref_sync_service_->IsPrefSynced(pref_name);
  }

  bool IsRegistered(const std::string& pref_name) {
    return pref_sync_service_->IsPrefRegistered(pref_name.c_str());
  }

  PrefService* GetPrefs() { return &prefs_; }
  TestingPrefServiceSyncable* GetTestingPrefService() { return &prefs_; }

 protected:
  TestingPrefServiceSyncable prefs_;

  PrefModelAssociator* pref_sync_service_;

  int next_pref_remote_sync_node_id_;
};

TEST_F(PrefServiceSyncableTest, CreatePrefSyncData) {
  prefs_.SetString(kStringPrefName, kExampleUrl0);

  const PrefService::Preference* pref = prefs_.FindPreference(kStringPrefName);
  syncer::SyncData sync_data;
  EXPECT_TRUE(pref_sync_service_->CreatePrefSyncData(
      pref->name(), *pref->GetValue(), &sync_data));
  EXPECT_EQ(std::string(kStringPrefName),
            syncer::SyncDataLocal(sync_data).GetTag());
  const sync_pb::PreferenceSpecifics& specifics(
      sync_data.GetSpecifics().preference());
  EXPECT_EQ(std::string(kStringPrefName), specifics.name());

  std::unique_ptr<base::Value> value =
      base::JSONReader::Read(specifics.value());
  EXPECT_TRUE(pref->GetValue()->Equals(value.get()));
}

TEST_F(PrefServiceSyncableTest, ModelAssociationDoNotSyncDefaults) {
  const PrefService::Preference* pref = prefs_.FindPreference(kStringPrefName);
  EXPECT_TRUE(pref->IsDefaultValue());
  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);

  EXPECT_TRUE(IsRegistered(kStringPrefName));
  EXPECT_TRUE(pref->IsDefaultValue());
  EXPECT_FALSE(FindValue(kStringPrefName, out).get());
}

TEST_F(PrefServiceSyncableTest, ModelAssociationEmptyCloud) {
  prefs_.SetString(kStringPrefName, kExampleUrl0);
  {
    ListPrefUpdate update(GetPrefs(), kListPrefName);
    base::ListValue* url_list = update.Get();
    url_list->AppendString(kExampleUrl0);
    url_list->AppendString(kExampleUrl1);
  }
  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);

  std::unique_ptr<base::Value> value(FindValue(kStringPrefName, out));
  ASSERT_TRUE(value.get());
  EXPECT_TRUE(GetPreferenceValue(kStringPrefName).Equals(value.get()));
  value = FindValue(kListPrefName, out);
  ASSERT_TRUE(value.get());
  EXPECT_TRUE(GetPreferenceValue(kListPrefName).Equals(value.get()));
}

TEST_F(PrefServiceSyncableTest, ModelAssociationCloudHasData) {
  prefs_.SetString(kStringPrefName, kExampleUrl0);
  {
    ListPrefUpdate update(GetPrefs(), kListPrefName);
    base::ListValue* url_list = update.Get();
    url_list->AppendString(kExampleUrl0);
  }

  syncer::SyncDataList in;
  syncer::SyncChangeList out;
  AddToRemoteDataList(kStringPrefName, base::Value(kExampleUrl1), &in);
  base::ListValue urls_to_restore;
  urls_to_restore.AppendString(kExampleUrl1);
  AddToRemoteDataList(kListPrefName, urls_to_restore, &in);
  AddToRemoteDataList(kDefaultCharsetPrefName,
                      base::Value(kNonDefaultCharsetValue), &in);
  InitWithSyncDataTakeOutput(in, &out);

  ASSERT_FALSE(FindValue(kStringPrefName, out).get());
  ASSERT_FALSE(FindValue(kDefaultCharsetPrefName, out).get());

  EXPECT_EQ(kExampleUrl1, prefs_.GetString(kStringPrefName));

  // No associator client is registered, so lists and dictionaries should not
  // get merged (remote write wins).
  auto expected_urls = std::make_unique<base::ListValue>();
  expected_urls->AppendString(kExampleUrl1);
  EXPECT_FALSE(FindValue(kListPrefName, out));
  EXPECT_TRUE(GetPreferenceValue(kListPrefName).Equals(expected_urls.get()));
  EXPECT_EQ(kNonDefaultCharsetValue, prefs_.GetString(kDefaultCharsetPrefName));
}

// Verifies that the implementation gracefully handles an initial remote sync
// data of wrong type. The local version should not get modified in these cases.
TEST_F(PrefServiceSyncableTest, ModelAssociationWithDataTypeMismatch) {
  base::HistogramTester histogram_tester;
  prefs_.SetString(kStringPrefName, kExampleUrl0);

  syncer::SyncDataList in;
  base::Value remote_int_value(123);
  AddToRemoteDataList(kStringPrefName, remote_int_value, &in);
  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(in, &out);
  EXPECT_THAT(out, IsEmpty());
  histogram_tester.ExpectBucketCount("Sync.Preferences.RemotePrefTypeMismatch",
                                     true, 1);
  EXPECT_THAT(prefs_.GetString(kStringPrefName), Eq(kExampleUrl0));
}

class TestPrefModelAssociatorClient : public PrefModelAssociatorClient {
 public:
  TestPrefModelAssociatorClient() {}
  ~TestPrefModelAssociatorClient() override {}

  // PrefModelAssociatorClient implementation.
  bool IsMergeableListPreference(const std::string& pref_name) const override {
    return pref_name == kListPrefName;
  }

  bool IsMergeableDictionaryPreference(
      const std::string& pref_name) const override {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestPrefModelAssociatorClient);
};

class PrefServiceSyncableMergeTest : public testing::Test {
 public:
  PrefServiceSyncableMergeTest()
      : pref_registry_(
            base::MakeRefCounted<user_prefs::PrefRegistrySyncable>()),
        pref_notifier_(new PrefNotifierImpl),
        managed_prefs_(base::MakeRefCounted<TestingPrefStore>()),
        user_prefs_(base::MakeRefCounted<TestingPrefStore>()),
        prefs_(
            std::unique_ptr<PrefNotifierImpl>(pref_notifier_),
            std::make_unique<PrefValueStore>(managed_prefs_.get(),
                                             new TestingPrefStore,
                                             new TestingPrefStore,
                                             new TestingPrefStore,
                                             user_prefs_.get(),
                                             new TestingPrefStore,
                                             pref_registry_->defaults().get(),
                                             pref_notifier_),
            user_prefs_,
            pref_registry_,
            &client_,
            base::BindRepeating(&PrefServiceSyncableMergeTest::HandleReadError),
            /*async=*/false),
        pref_sync_service_(nullptr),
        next_pref_remote_sync_node_id_(0) {}

  void SetUp() override {
    pref_registry_->RegisterStringPref(kUnsyncedPreferenceName,
                                       kUnsyncedPreferenceDefaultValue);
    pref_registry_->RegisterStringPref(
        kStringPrefName, std::string(),
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
    pref_registry_->RegisterListPref(
        kListPrefName, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
    pref_registry_->RegisterDictionaryPref(
        kDictPrefName, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
    pref_registry_->RegisterStringPref(
        kDefaultCharsetPrefName, kDefaultCharsetValue,
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

    // Downcast to PrefModelAssociator so that tests can access its' specific
    // behavior. This is a smell. The roles between PrefServiceSyncable and
    // PrefModelAssociator are not clearly separated (and this test should only
    // test against the SyncableService interface).
    pref_sync_service_ =  // static_cast<PrefModelAssociator*>(
        prefs_.GetSyncableService(syncer::PREFERENCES);  //);
    ASSERT_THAT(pref_sync_service_, NotNull());
  }

  /// Empty stub for prefs_ error handling.
  static void HandleReadError(PersistentPrefStore::PrefReadError error) {}

  syncer::SyncChange MakeRemoteChange(int64_t id,
                                      const std::string& name,
                                      const base::Value& value,
                                      SyncChange::SyncChangeType type) {
    std::string serialized;
    JSONStringValueSerializer json(&serialized);
    CHECK(json.Serialize(value));
    sync_pb::EntitySpecifics entity;
    sync_pb::PreferenceSpecifics* pref_one = entity.mutable_preference();
    pref_one->set_name(name);
    pref_one->set_value(serialized);
    return syncer::SyncChange(
        FROM_HERE, type,
        syncer::SyncData::CreateRemoteData(id, entity, base::Time()));
  }

  void AddToRemoteDataList(const std::string& name,
                           const base::Value& value,
                           syncer::SyncDataList* out) {
    std::string serialized;
    JSONStringValueSerializer json(&serialized);
    ASSERT_TRUE(json.Serialize(value));
    sync_pb::EntitySpecifics one;
    sync_pb::PreferenceSpecifics* pref_one = one.mutable_preference();
    pref_one->set_name(name);
    pref_one->set_value(serialized);
    out->push_back(SyncData::CreateRemoteData(++next_pref_remote_sync_node_id_,
                                              one, base::Time()));
  }

  void InitWithSyncDataTakeOutput(const syncer::SyncDataList& initial_data,
                                  syncer::SyncChangeList* output) {
    syncer::SyncMergeResult r = pref_sync_service_->MergeDataAndStartSyncing(
        syncer::PREFERENCES, initial_data,
        std::make_unique<TestSyncProcessorStub>(output),
        std::make_unique<syncer::SyncErrorFactoryMock>());
    EXPECT_FALSE(r.error().IsSet());
  }

  const base::Value& GetPreferenceValue(const std::string& name) {
    const PrefService::Preference* preference =
        prefs_.FindPreference(name.c_str());
    return *preference->GetValue();
  }

  std::unique_ptr<base::Value> FindValue(const std::string& name,
                                         const syncer::SyncChangeList& list) {
    auto it = list.begin();
    for (; it != list.end(); ++it) {
      if (syncer::SyncDataLocal(it->sync_data()).GetTag() == name) {
        return base::JSONReader::Read(
            it->sync_data().GetSpecifics().preference().value());
      }
    }
    return nullptr;
  }

 protected:
  scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry_;
  // Owned by prefs_;
  PrefNotifierImpl* pref_notifier_;
  scoped_refptr<TestingPrefStore> managed_prefs_;
  scoped_refptr<TestingPrefStore> user_prefs_;
  TestPrefModelAssociatorClient client_;
  PrefServiceSyncable prefs_;
  syncer::SyncableService* pref_sync_service_;
  int next_pref_remote_sync_node_id_;
};

TEST_F(PrefServiceSyncableMergeTest, ShouldMergeSelectedListValues) {
  {
    ListPrefUpdate update(&prefs_, kListPrefName);
    base::ListValue* url_list = update.Get();
    url_list->AppendString(kExampleUrl0);
    url_list->AppendString(kExampleUrl1);
  }

  base::ListValue urls_to_restore;
  urls_to_restore.AppendString(kExampleUrl1);
  urls_to_restore.AppendString(kExampleUrl2);
  syncer::SyncDataList in;
  AddToRemoteDataList(kListPrefName, urls_to_restore, &in);

  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(in, &out);

  std::unique_ptr<base::ListValue> expected_urls(new base::ListValue);
  expected_urls->AppendString(kExampleUrl1);
  expected_urls->AppendString(kExampleUrl2);
  expected_urls->AppendString(kExampleUrl0);
  std::unique_ptr<base::Value> value(FindValue(kListPrefName, out));
  ASSERT_TRUE(value.get());
  EXPECT_TRUE(value->Equals(expected_urls.get())) << *value;
  EXPECT_TRUE(GetPreferenceValue(kListPrefName).Equals(expected_urls.get()));
}

// List preferences have special handling at association time due to our ability
// to merge the local and sync value. Make sure the merge logic doesn't merge
// managed preferences.
TEST_F(PrefServiceSyncableMergeTest, ManagedListPreferences) {
  // Make the list of urls to restore on startup managed.
  base::ListValue managed_value;
  managed_value.AppendString(kExampleUrl0);
  managed_value.AppendString(kExampleUrl1);
  managed_prefs_->SetValue(kListPrefName, managed_value.CreateDeepCopy(),
                           WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);

  // Set a cloud version.
  syncer::SyncDataList in;
  base::ListValue urls_to_restore;
  urls_to_restore.AppendString(kExampleUrl1);
  urls_to_restore.AppendString(kExampleUrl2);
  AddToRemoteDataList(kListPrefName, urls_to_restore, &in);

  // Start sync and verify the synced value didn't get merged.
  {
    syncer::SyncChangeList out;
    InitWithSyncDataTakeOutput(in, &out);
    EXPECT_FALSE(FindValue(kListPrefName, out).get());
  }

  // Changing the user's urls to restore on startup pref should not sync
  // anything.
  {
    syncer::SyncChangeList out;
    base::ListValue user_value;
    user_value.AppendString("http://chromium.org");
    prefs_.Set(kListPrefName, user_value);
    EXPECT_FALSE(FindValue(kListPrefName, out).get());
  }

  // An incoming sync transaction should change the user value, not the managed
  // value.
  base::ListValue sync_value;
  sync_value.AppendString("http://crbug.com");
  syncer::SyncChangeList list;
  list.push_back(MakeRemoteChange(1, kListPrefName, sync_value,
                                  SyncChange::ACTION_UPDATE));
  pref_sync_service_->ProcessSyncChanges(FROM_HERE, list);

  const base::Value* managed_prefs_result;
  ASSERT_TRUE(managed_prefs_->GetValue(kListPrefName, &managed_prefs_result));
  EXPECT_TRUE(managed_value.Equals(managed_prefs_result));
  // Get should return the managed value, too.
  EXPECT_TRUE(managed_value.Equals(prefs_.Get(kListPrefName)));
  // Verify the user pref value has the change.
  EXPECT_TRUE(sync_value.Equals(prefs_.GetUserPrefValue(kListPrefName)));
}

TEST_F(PrefServiceSyncableMergeTest, ShouldMergeSelectedDictionaryValues) {
  {
    DictionaryPrefUpdate update(&prefs_, kDictPrefName);
    base::DictionaryValue* dict_value = update.Get();
    dict_value->Set("my_key1", std::make_unique<base::Value>("my_value1"));
    dict_value->Set("my_key3", std::make_unique<base::Value>("my_value3"));
  }

  base::DictionaryValue remote_update;
  remote_update.Set("my_key2", std::make_unique<base::Value>("my_value2"));
  syncer::SyncDataList in;
  AddToRemoteDataList(kDictPrefName, remote_update, &in);

  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(in, &out);

  base::DictionaryValue expected_dict;
  expected_dict.Set("my_key1", std::make_unique<base::Value>("my_value1"));
  expected_dict.Set("my_key2", std::make_unique<base::Value>("my_value2"));
  expected_dict.Set("my_key3", std::make_unique<base::Value>("my_value3"));
  std::unique_ptr<base::Value> value(FindValue(kDictPrefName, out));
  ASSERT_TRUE(value.get());
  EXPECT_TRUE(value->Equals(&expected_dict));
  EXPECT_TRUE(GetPreferenceValue(kDictPrefName).Equals(&expected_dict));
}

TEST_F(PrefServiceSyncableMergeTest, InitWithUnknownPrefsValue) {
  base::HistogramTester histogram_tester;
  const std::string pref_name1 = "testing.whitelisted_pref1";
  const std::string pref_name2 = "testing.whitelisted_pref2";
  pref_registry_->WhitelistLateRegistrationPrefForSync(pref_name1);
  pref_registry_->WhitelistLateRegistrationPrefForSync(pref_name2);

  syncer::SyncDataList in;
  AddToRemoteDataList(pref_name1, base::Value("remote_value1"), &in);
  AddToRemoteDataList(pref_name2, base::Value("remote_value2"), &in);
  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(in, &out);
  pref_registry_->RegisterStringPref(
      pref_name1, "default_value",
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  EXPECT_THAT(GetPreferenceValue(pref_name1).GetString(), Eq("remote_value1"));

  histogram_tester.ExpectBucketCount("Sync.Preferences.SyncingUnknownPrefs", 2,
                                     1);
}

TEST_F(PrefServiceSyncableMergeTest, ReceiveUnknownPrefsValue) {
  base::HistogramTester histogram_tester;
  const std::string pref_name = "testing.whitelisted_pref";
  pref_registry_->WhitelistLateRegistrationPrefForSync(pref_name);

  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);

  syncer::SyncChangeList remote_changes;
  remote_changes.push_back(MakeRemoteChange(
      1, pref_name, base::Value("remote_value"), SyncChange::ACTION_UPDATE));
  pref_sync_service_->ProcessSyncChanges(FROM_HERE, remote_changes);
  EXPECT_THAT(prefs_.IsPrefSynced(pref_name), Eq(true));

  pref_registry_->RegisterStringPref(
      pref_name, "default_value",
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  EXPECT_THAT(GetPreferenceValue(pref_name).GetString(), Eq("remote_value"));
}

TEST_F(PrefServiceSyncableMergeTest, KeepPriorityPreferencesSeparately) {
  base::HistogramTester histogram_tester;
  const std::string pref_name = "testing.priority_pref";
  pref_registry_->RegisterStringPref(
      pref_name, "priority-default",
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);

  syncer::SyncDataList in;
  // AddToRemoteDataList() produces sync data for non-priority prefs.
  AddToRemoteDataList(pref_name, base::Value("non-priority-value"), &in);
  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(in, &out);
  EXPECT_THAT(GetPreferenceValue(pref_name).GetString(),
              Eq("priority-default"));
}

class ShouldNotBeNotifedObserver : public SyncedPrefObserver {
 public:
  ShouldNotBeNotifedObserver() {}
  ~ShouldNotBeNotifedObserver() {}

  void OnSyncedPrefChanged(const std::string& path, bool from_sync) override {
    ADD_FAILURE() << "Unexpected notification about a pref change with path: '"
                  << path << "' and from_sync: " << from_sync;
  }
};

TEST_F(PrefServiceSyncableMergeTest, RegisterShouldClearTypeMismatchingData) {
  base::HistogramTester histogram_tester;
  const std::string pref_name = "testing.whitelisted_pref";
  pref_registry_->WhitelistLateRegistrationPrefForSync(pref_name);
  // Make sure no changes will be communicated to any synced pref listeners
  // (those listeners are typically only used for metrics but we still don't
  // want to inform them).
  ShouldNotBeNotifedObserver observer;
  prefs_.AddSyncedPrefObserver(pref_name, &observer);
  syncer::SyncDataList in;
  AddToRemoteDataList(pref_name, base::Value("remote_value"), &in);
  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(in, &out);
  ASSERT_THAT(out, IsEmpty());

  EXPECT_TRUE(user_prefs_->GetValue(pref_name, nullptr));

  pref_registry_->RegisterListPref(
      pref_name, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  EXPECT_TRUE(GetPreferenceValue(pref_name).GetList().empty());
  EXPECT_FALSE(user_prefs_->GetValue(pref_name, nullptr));
  // Make sure the removal of the value was not communicated to sync via the
  // SyncProcessor.
  EXPECT_THAT(out, IsEmpty());

  histogram_tester.ExpectBucketCount(
      "Sync.Preferences.ClearedLocalPrefOnTypeMismatch", true, 1);
  prefs_.RemoveSyncedPrefObserver(pref_name, &observer);
}

TEST_F(PrefServiceSyncableMergeTest, ShouldIgnoreUpdatesToNotSyncablePrefs) {
  const std::string pref_name = "testing.not_syncable_pref";
  pref_registry_->RegisterStringPref(pref_name, "default_value",
                                     PrefRegistry::NO_REGISTRATION_FLAGS);
  syncer::SyncDataList in;
  AddToRemoteDataList(pref_name, base::Value("remote_value"), &in);
  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(in, &out);
  EXPECT_THAT(GetPreferenceValue(pref_name).GetString(), Eq("default_value"));

  syncer::SyncChangeList remote_changes;
  remote_changes.push_back(MakeRemoteChange(
      1, pref_name, base::Value("remote_value2"), SyncChange::ACTION_UPDATE));
  pref_sync_service_->ProcessSyncChanges(FROM_HERE, remote_changes);
  EXPECT_THAT(prefs_.IsPrefSynced(pref_name), Eq(false));

  EXPECT_THAT(GetPreferenceValue(pref_name).GetString(), Eq("default_value"));
}

TEST_F(PrefServiceSyncableMergeTest, GetAllSyncDataForLateRegisteredPrefs) {
  const std::string pref_name = "testing.whitelisted_pref";
  pref_registry_->WhitelistLateRegistrationPrefForSync(pref_name);

  syncer::SyncDataList in;
  AddToRemoteDataList(pref_name, base::Value("remote_value"), &in);
  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(in, &out);

  syncer::SyncDataList all_data =
      prefs_.GetSyncableService(syncer::PREFERENCES)
          ->GetAllSyncData(syncer::PREFERENCES);
  EXPECT_THAT(all_data, IsEmpty());

  // Make sure the preference appears in the result once it's registered.
  pref_registry_->RegisterStringPref(
      pref_name, "default_value",
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  all_data = prefs_.GetSyncableService(syncer::PREFERENCES)
                 ->GetAllSyncData(syncer::PREFERENCES);
  ASSERT_THAT(all_data, SizeIs(1));
  EXPECT_THAT(all_data[0].GetSpecifics().preference().name(), Eq(pref_name));
  EXPECT_THAT(all_data[0].GetSpecifics().preference().value(),
              Eq("\"remote_value\""));
}

TEST_F(PrefServiceSyncableTest, FailModelAssociation) {
  syncer::SyncChangeList output;
  TestSyncProcessorStub* stub = new TestSyncProcessorStub(&output);
  stub->FailNextProcessSyncChanges();
  syncer::SyncMergeResult r = pref_sync_service_->MergeDataAndStartSyncing(
      syncer::PREFERENCES, syncer::SyncDataList(), base::WrapUnique(stub),
      std::make_unique<syncer::SyncErrorFactoryMock>());
  EXPECT_TRUE(r.error().IsSet());
}

TEST_F(PrefServiceSyncableTest, UpdatedPreferenceWithDefaultValue) {
  const PrefService::Preference* pref = prefs_.FindPreference(kStringPrefName);
  EXPECT_TRUE(pref->IsDefaultValue());

  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);
  out.clear();

  base::Value expected(kExampleUrl0);
  GetPrefs()->Set(kStringPrefName, expected);

  std::unique_ptr<base::Value> actual(FindValue(kStringPrefName, out));
  ASSERT_TRUE(actual.get());
  EXPECT_TRUE(expected.Equals(actual.get()));
}

TEST_F(PrefServiceSyncableTest, UpdatedPreferenceWithValue) {
  GetPrefs()->SetString(kStringPrefName, kExampleUrl0);
  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);
  out.clear();

  base::Value expected(kExampleUrl1);
  GetPrefs()->Set(kStringPrefName, expected);

  std::unique_ptr<base::Value> actual(FindValue(kStringPrefName, out));
  ASSERT_TRUE(actual.get());
  EXPECT_TRUE(expected.Equals(actual.get()));
}

TEST_F(PrefServiceSyncableTest, UpdatedSyncNodeActionUpdate) {
  GetPrefs()->SetString(kStringPrefName, kExampleUrl0);
  InitWithNoSyncData();

  base::Value expected(kExampleUrl1);
  syncer::SyncChangeList list;
  list.push_back(MakeRemoteChange(1, kStringPrefName, expected,
                                  SyncChange::ACTION_UPDATE));
  pref_sync_service_->ProcessSyncChanges(FROM_HERE, list);

  const base::Value& actual = GetPreferenceValue(kStringPrefName);
  EXPECT_TRUE(expected.Equals(&actual));
}

// Verifies that the implementation gracefully handles a remote update with the
// wrong type. The local version should not get modified in these cases.
TEST_F(PrefServiceSyncableTest, UpdatedSyncNodeActionUpdateTypeMismatch) {
  base::HistogramTester histogram_tester;
  GetPrefs()->SetString(kStringPrefName, kExampleUrl0);
  InitWithNoSyncData();

  base::Value remote_int_value(123);
  syncer::SyncChangeList remote_changes;
  remote_changes.push_back(MakeRemoteChange(
      1, kStringPrefName, remote_int_value, SyncChange::ACTION_UPDATE));
  pref_sync_service_->ProcessSyncChanges(FROM_HERE, remote_changes);

  EXPECT_THAT(prefs_.GetString(kStringPrefName), Eq(kExampleUrl0));
  histogram_tester.ExpectBucketCount("Sync.Preferences.RemotePrefTypeMismatch",
                                     true, 1);
}

TEST_F(PrefServiceSyncableTest, UpdatedSyncNodeActionAdd) {
  InitWithNoSyncData();

  base::Value expected(kExampleUrl0);
  syncer::SyncChangeList list;
  list.push_back(
      MakeRemoteChange(1, kStringPrefName, expected, SyncChange::ACTION_ADD));
  pref_sync_service_->ProcessSyncChanges(FROM_HERE, list);

  const base::Value& actual = GetPreferenceValue(kStringPrefName);
  EXPECT_TRUE(expected.Equals(&actual));
  EXPECT_TRUE(pref_sync_service_->IsPrefSynced(kStringPrefName));
}

TEST_F(PrefServiceSyncableTest, UpdatedSyncNodeUnknownPreference) {
  InitWithNoSyncData();
  syncer::SyncChangeList list;
  base::Value expected(kExampleUrl0);
  list.push_back(MakeRemoteChange(1, "unknown preference", expected,
                                  SyncChange::ACTION_UPDATE));
  pref_sync_service_->ProcessSyncChanges(FROM_HERE, list);
  // Nothing interesting happens on the client when it gets an update
  // of an unknown preference.  We just should not crash.
}

TEST_F(PrefServiceSyncableTest, ManagedPreferences) {
  // Make the homepage preference managed.
  base::Value managed_value("http://example.com");
  prefs_.SetManagedPref(kStringPrefName, managed_value.CreateDeepCopy());

  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);
  out.clear();

  // Changing the homepage preference should not sync anything.
  base::Value user_value("http://chromium..com");
  prefs_.SetUserPref(kStringPrefName, user_value.CreateDeepCopy());
  EXPECT_TRUE(out.empty());

  // An incoming sync transaction should change the user value, not the managed
  // value.
  base::Value sync_value("http://crbug.com");
  syncer::SyncChangeList list;
  list.push_back(MakeRemoteChange(1, kStringPrefName, sync_value,
                                  SyncChange::ACTION_UPDATE));
  pref_sync_service_->ProcessSyncChanges(FROM_HERE, list);

  EXPECT_TRUE(managed_value.Equals(prefs_.GetManagedPref(kStringPrefName)));
  EXPECT_TRUE(sync_value.Equals(prefs_.GetUserPref(kStringPrefName)));
}

TEST_F(PrefServiceSyncableTest, DynamicManagedPreferences) {
  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);
  out.clear();
  base::Value initial_value("http://example.com/initial");
  GetPrefs()->Set(kStringPrefName, initial_value);
  std::unique_ptr<base::Value> actual(FindValue(kStringPrefName, out));
  ASSERT_TRUE(actual.get());
  EXPECT_TRUE(initial_value.Equals(actual.get()));

  // Switch kHomePage to managed and set a different value.
  base::Value managed_value("http://example.com/managed");
  GetTestingPrefService()->SetManagedPref(kStringPrefName,
                                          managed_value.CreateDeepCopy());

  // The pref value should be the one dictated by policy.
  EXPECT_TRUE(managed_value.Equals(&GetPreferenceValue(kStringPrefName)));

  // Switch kHomePage back to unmanaged.
  GetTestingPrefService()->RemoveManagedPref(kStringPrefName);

  // The original value should be picked up.
  EXPECT_TRUE(initial_value.Equals(&GetPreferenceValue(kStringPrefName)));
}

TEST_F(PrefServiceSyncableTest, DynamicManagedPreferencesWithSyncChange) {
  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);
  out.clear();

  base::Value initial_value("http://example.com/initial");
  GetPrefs()->Set(kStringPrefName, initial_value);
  std::unique_ptr<base::Value> actual(FindValue(kStringPrefName, out));
  EXPECT_TRUE(initial_value.Equals(actual.get()));

  // Switch kHomePage to managed and set a different value.
  base::Value managed_value("http://example.com/managed");
  GetTestingPrefService()->SetManagedPref(kStringPrefName,
                                          managed_value.CreateDeepCopy());

  // Change the sync value.
  base::Value sync_value("http://example.com/sync");
  syncer::SyncChangeList list;
  list.push_back(MakeRemoteChange(1, kStringPrefName, sync_value,
                                  SyncChange::ACTION_UPDATE));
  pref_sync_service_->ProcessSyncChanges(FROM_HERE, list);

  // The pref value should still be the one dictated by policy.
  EXPECT_TRUE(managed_value.Equals(&GetPreferenceValue(kStringPrefName)));

  // Switch kHomePage back to unmanaged.
  GetTestingPrefService()->RemoveManagedPref(kStringPrefName);

  // Sync value should be picked up.
  EXPECT_TRUE(sync_value.Equals(&GetPreferenceValue(kStringPrefName)));
}

TEST_F(PrefServiceSyncableTest, DynamicManagedDefaultPreferences) {
  const PrefService::Preference* pref = prefs_.FindPreference(kStringPrefName);
  EXPECT_TRUE(pref->IsDefaultValue());
  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);

  EXPECT_TRUE(IsRegistered(kStringPrefName));
  EXPECT_TRUE(pref->IsDefaultValue());
  EXPECT_FALSE(FindValue(kStringPrefName, out).get());
  out.clear();

  // Switch kHomePage to managed and set a different value.
  base::Value managed_value("http://example.com/managed");
  GetTestingPrefService()->SetManagedPref(kStringPrefName,
                                          managed_value.CreateDeepCopy());
  // The pref value should be the one dictated by policy.
  EXPECT_TRUE(managed_value.Equals(&GetPreferenceValue(kStringPrefName)));
  EXPECT_FALSE(pref->IsDefaultValue());
  // There should be no synced value.
  EXPECT_FALSE(FindValue(kStringPrefName, out).get());
  // Switch kHomePage back to unmanaged.
  GetTestingPrefService()->RemoveManagedPref(kStringPrefName);
  // The original value should be picked up.
  EXPECT_TRUE(pref->IsDefaultValue());
  // There should still be no synced value.
  EXPECT_FALSE(FindValue(kStringPrefName, out).get());
}

TEST_F(PrefServiceSyncableTest, DeletePreference) {
  prefs_.SetString(kStringPrefName, kExampleUrl0);
  const PrefService::Preference* pref = prefs_.FindPreference(kStringPrefName);
  EXPECT_FALSE(pref->IsDefaultValue());

  InitWithNoSyncData();

  auto null_value = std::make_unique<base::Value>();
  syncer::SyncChangeList list;
  list.push_back(MakeRemoteChange(1, kStringPrefName, *null_value,
                                  SyncChange::ACTION_DELETE));
  pref_sync_service_->ProcessSyncChanges(FROM_HERE, list);
  EXPECT_TRUE(pref->IsDefaultValue());
}

TEST_F(PrefServiceSyncableTest, RegisterMergeDataFinishedCallback) {
  int num_callbacks = 0;

  prefs_.RegisterMergeDataFinishedCallback(
      base::Bind(&Increment, &num_callbacks));
  EXPECT_EQ(0, num_callbacks);

  InitWithNoSyncData();
  EXPECT_EQ(1, num_callbacks);
}

}  // namespace

}  // namespace sync_preferences
