// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/pref_service_syncable.h"

#include <stdint.h>

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_store.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/preference_specifics.pb.h"
#include "components/sync_preferences/pref_model_associator.h"
#include "components/sync_preferences/pref_model_associator_client.h"
#include "components/sync_preferences/pref_service_syncable_factory.h"
#include "components/sync_preferences/pref_service_syncable_observer.h"
#include "components/sync_preferences/syncable_prefs_database.h"
#include "components/sync_preferences/synced_pref_observer.h"
#include "components/sync_preferences/test_syncable_prefs_database.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "testing/gmock/include/gmock/gmock-matchers.h"
#endif

using syncer::DataType;
using syncer::DataTypeSet;
using syncer::SyncChange;
using syncer::SyncData;
using testing::Eq;
using testing::IsEmpty;
using testing::Matches;
using testing::NotNull;
using testing::UnorderedElementsAre;
using user_prefs::PrefRegistrySyncable;

namespace sync_preferences {

namespace {

const char kExampleUrl0[] = "http://example.com/0";
const char kExampleUrl1[] = "http://example.com/1";
const char kExampleUrl2[] = "http://example.com/2";
const char kStringPrefName[] = "string_pref_name";
const char kListPrefName[] = "list_pref_name";
const char kMergeableListPrefName[] = "mergeable.list_pref_name";
const char kMergeableDictPrefName[] = "mergeable.dict_pref_name";
const char kUnsyncedPreferenceName[] = "nonsense_pref_name";
const char kUnsyncedPreferenceDefaultValue[] = "default";
const char kDefaultCharsetPrefName[] = "default_charset";
const char kNonDefaultCharsetValue[] = "foo";
const char kDefaultCharsetValue[] = "utf-8";
const char kBrowserPrefName[] = "browser_pref";
const char kBrowserPriorityPrefName[] = "browser_priority_pref";
#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kOsPrefName[] = "os_pref";
const char kOsPriorityPrefName[] = "os_priority_pref";
#endif

// Assigning an id of 0 to all the test prefs.
const TestSyncablePrefsDatabase::PrefsMap kSyncablePrefsDatabase = {
    {kStringPrefName,
     {1, syncer::PREFERENCES, PrefSensitivity::kNone, MergeBehavior::kNone}},
    {kListPrefName,
     {2, syncer::PREFERENCES, PrefSensitivity::kNone, MergeBehavior::kNone}},
    {kMergeableListPrefName,
     {3, syncer::PREFERENCES, PrefSensitivity::kNone,
      MergeBehavior::kMergeableListWithRewriteOnUpdate}},
    {kMergeableDictPrefName,
     {4, syncer::PREFERENCES, PrefSensitivity::kNone,
      MergeBehavior::kMergeableDict}},
    {kDefaultCharsetPrefName,
     {5, syncer::PREFERENCES, PrefSensitivity::kNone, MergeBehavior::kNone}},
    {kBrowserPriorityPrefName,
     {6, syncer::PRIORITY_PREFERENCES, PrefSensitivity::kNone,
      MergeBehavior::kNone}},
    {kBrowserPrefName,
     {7, syncer::PREFERENCES, PrefSensitivity::kNone, MergeBehavior::kNone}},
    {kBrowserPriorityPrefName,
     {8, syncer::PRIORITY_PREFERENCES, PrefSensitivity::kNone,
      MergeBehavior::kNone}},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {kOsPrefName,
     {9, syncer::OS_PREFERENCES, PrefSensitivity::kNone, MergeBehavior::kNone}},
    {kOsPriorityPrefName,
     {10, syncer::OS_PRIORITY_PREFERENCES, PrefSensitivity::kNone,
      MergeBehavior::kNone}},
#endif
};

// Searches for a preference matching `name` and, if specified,`change_type`,
// within `list`. Returns the value of the first matching pref, or nullopt if
// none is found.
std::optional<base::Value> FindValue(
    const std::string& name,
    const syncer::SyncChangeList& list,
    std::optional<syncer::SyncChange::SyncChangeType> change_type =
        std::nullopt) {
  for (const SyncChange& change : list) {
    if ((!change_type || change.change_type() == *change_type) &&
        change.sync_data().GetClientTagHash() ==
            syncer::ClientTagHash::FromUnhashed(syncer::PREFERENCES, name)) {
      return base::JSONReader::Read(
          change.sync_data().GetSpecifics().preference().value());
    }
  }
  return std::nullopt;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr DataTypeSet kAllPreferenceDataTypes = {
    syncer::PREFERENCES, syncer::PRIORITY_PREFERENCES, syncer::OS_PREFERENCES,
    syncer::OS_PRIORITY_PREFERENCES};

MATCHER_P(MatchesDataType, data_type, "") {
  const syncer::SyncChange& sync_change = arg;
  return Matches(data_type)(sync_change.sync_data().GetDataType());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class TestSyncProcessorStub : public syncer::SyncChangeProcessor {
 public:
  explicit TestSyncProcessorStub(syncer::SyncChangeList* output)
      : output_(output) {}

  std::optional<syncer::ModelError> ProcessSyncChanges(
      const base::Location& from_here,
      const syncer::SyncChangeList& change_list) override {
    if (output_) {
      output_->insert(output_->end(), change_list.begin(), change_list.end());
    }
    if (fail_next_) {
      fail_next_ = false;
      return syncer::ModelError(FROM_HERE, "Error");
    }
    return std::nullopt;
  }

  void FailNextProcessSyncChanges() { fail_next_ = true; }

 private:
  raw_ptr<syncer::SyncChangeList> output_;
  bool fail_next_ = false;
};

class TestSyncedPrefObserver : public SyncedPrefObserver {
 public:
  TestSyncedPrefObserver() = default;
  ~TestSyncedPrefObserver() = default;

  void OnSyncedPrefChanged(std::string_view path, bool from_sync) override {
    last_pref_ = std::string(path);
    changed_count_++;
  }

  void OnStartedSyncing(std::string_view path) override {
    synced_pref_ = std::string(path);
    sync_started_count_++;
  }

  std::string last_pref_;
  int changed_count_ = 0;

  std::string synced_pref_;
  int sync_started_count_ = 0;
};

class TestPrefServiceSyncableObserver : public PrefServiceSyncableObserver {
 public:
  TestPrefServiceSyncableObserver() = default;
  ~TestPrefServiceSyncableObserver() override = default;

  void OnIsSyncingChanged() override {
    if (sync_pref_observer_ && sync_pref_observer_->sync_started_count_ > 0) {
      is_syncing_changed_ = true;
    }
  }

  void SetSyncedPrefObserver(const TestSyncedPrefObserver* sync_pref_observer) {
    sync_pref_observer_ = sync_pref_observer;
  }

  bool is_syncing_changed() { return is_syncing_changed_; }

 private:
  bool is_syncing_changed_ = false;
  raw_ptr<const TestSyncedPrefObserver> sync_pref_observer_ = nullptr;
};

syncer::SyncChange MakeRemoteChange(const std::string& name,
                                    base::ValueView value,
                                    SyncChange::SyncChangeType change_type,
                                    syncer::DataType data_type) {
  std::string serialized;
  JSONStringValueSerializer json(&serialized);
  bool success = json.Serialize(value);
  DCHECK(success);
  sync_pb::EntitySpecifics entity;
  sync_pb::PreferenceSpecifics* pref =
      PrefModelAssociator::GetMutableSpecifics(data_type, &entity);
  pref->set_name(name);
  pref->set_value(serialized);
  return syncer::SyncChange(
      FROM_HERE, change_type,
      syncer::SyncData::CreateRemoteData(
          entity, syncer::ClientTagHash::FromUnhashed(data_type, name)));
}

// Creates a SyncChange for data type |PREFERENCES|.
syncer::SyncChange MakeRemoteChange(const std::string& name,
                                    base::ValueView value,
                                    SyncChange::SyncChangeType type) {
  return MakeRemoteChange(name, value, type, syncer::DataType::PREFERENCES);
}

// Creates SyncData for a remote pref change.
SyncData CreateRemoteSyncData(const std::string& name, base::ValueView value) {
  std::string serialized;
  JSONStringValueSerializer json(&serialized);
  EXPECT_TRUE(json.Serialize(value));
  sync_pb::EntitySpecifics one;
  sync_pb::PreferenceSpecifics* pref_one = one.mutable_preference();
  pref_one->set_name(name);
  pref_one->set_value(serialized);
  return SyncData::CreateRemoteData(
      one,
      syncer::ClientTagHash::FromUnhashed(syncer::DataType::PREFERENCES, name));
}

class PrefServiceSyncableTest : public testing::Test {
 public:
  PrefServiceSyncableTest() = default;

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

  void AddToRemoteDataList(const std::string& name,
                           base::ValueView value,
                           syncer::SyncDataList* out) {
    out->push_back(CreateRemoteSyncData(name, value));
  }

  void InitWithSyncDataTakeOutput(const syncer::SyncDataList& initial_data,
                                  syncer::SyncChangeList* output) {
    std::optional<syncer::ModelError> error =
        pref_sync_service_->MergeDataAndStartSyncing(
            syncer::PREFERENCES, initial_data,
            std::make_unique<TestSyncProcessorStub>(output));
    EXPECT_FALSE(error.has_value());
  }

  void InitWithNoSyncData() {
    InitWithSyncDataTakeOutput(syncer::SyncDataList(), nullptr);
  }

  const base::Value& GetPreferenceValue(const std::string& name) {
    const PrefService::Preference* preference =
        prefs_.FindPreference(name.c_str());
    return *preference->GetValue();
  }

  bool IsRegistered(const std::string& pref_name) {
    return pref_sync_service_->IsPrefRegistered(pref_name.c_str());
  }

  PrefService* GetPrefs() { return &prefs_; }
  TestingPrefServiceSyncable* GetTestingPrefService() { return &prefs_; }

 protected:
  TestingPrefServiceSyncable prefs_;

  raw_ptr<PrefModelAssociator> pref_sync_service_ = nullptr;
};

TEST_F(PrefServiceSyncableTest, CreatePrefSyncData) {
  prefs_.SetString(kStringPrefName, kExampleUrl0);

  const PrefService::Preference* pref = prefs_.FindPreference(kStringPrefName);
  syncer::SyncData sync_data;
  EXPECT_TRUE(pref_sync_service_->CreatePrefSyncData(
      pref->name(), *pref->GetValue(), &sync_data));
  EXPECT_EQ(
      syncer::ClientTagHash::FromUnhashed(syncer::PREFERENCES, kStringPrefName),
      sync_data.GetClientTagHash());
  const sync_pb::PreferenceSpecifics& specifics(
      sync_data.GetSpecifics().preference());
  EXPECT_EQ(std::string(kStringPrefName), specifics.name());

  std::optional<base::Value> value = base::JSONReader::Read(specifics.value());
  EXPECT_EQ(*pref->GetValue(), *value);
}

TEST_F(PrefServiceSyncableTest, ModelAssociationDoNotSyncDefaults) {
  const PrefService::Preference* pref = prefs_.FindPreference(kStringPrefName);
  EXPECT_TRUE(pref->IsDefaultValue());
  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);

  EXPECT_TRUE(IsRegistered(kStringPrefName));
  EXPECT_TRUE(pref->IsDefaultValue());
  EXPECT_FALSE(FindValue(kStringPrefName, out));
}

TEST_F(PrefServiceSyncableTest, ModelAssociationEmptyCloud) {
  prefs_.SetString(kStringPrefName, kExampleUrl0);
  {
    ScopedListPrefUpdate update(GetPrefs(), kListPrefName);
    update->Append(kExampleUrl0);
    update->Append(kExampleUrl1);
  }
  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);

  std::optional<base::Value> value(FindValue(kStringPrefName, out));
  ASSERT_TRUE(value);
  EXPECT_EQ(GetPreferenceValue(kStringPrefName), *value);
  value = FindValue(kListPrefName, out);
  ASSERT_TRUE(value);
  EXPECT_EQ(GetPreferenceValue(kListPrefName), *value);
}

TEST_F(PrefServiceSyncableTest, ModelAssociationCloudHasData) {
  prefs_.SetString(kStringPrefName, kExampleUrl0);
  {
    ScopedListPrefUpdate update(GetPrefs(), kListPrefName);
    update->Append(kExampleUrl0);
  }

  syncer::SyncDataList in;
  syncer::SyncChangeList out;
  AddToRemoteDataList(kStringPrefName, base::Value(kExampleUrl1), &in);
  auto urls_to_restore = base::Value::List().Append(kExampleUrl1);
  AddToRemoteDataList(kListPrefName, urls_to_restore, &in);
  AddToRemoteDataList(kDefaultCharsetPrefName,
                      base::Value(kNonDefaultCharsetValue), &in);
  InitWithSyncDataTakeOutput(in, &out);

  ASSERT_FALSE(FindValue(kStringPrefName, out));
  ASSERT_FALSE(FindValue(kDefaultCharsetPrefName, out));

  EXPECT_EQ(kExampleUrl1, prefs_.GetString(kStringPrefName));

  // No associator client is registered, so lists and dictionaries should not
  // get merged (remote write wins).
  auto expected_urls = base::Value::List().Append(kExampleUrl1);
  EXPECT_FALSE(FindValue(kListPrefName, out));
  EXPECT_EQ(GetPreferenceValue(kListPrefName), expected_urls);
  EXPECT_EQ(kNonDefaultCharsetValue, prefs_.GetString(kDefaultCharsetPrefName));
}

// Verifies that the implementation gracefully handles an initial remote sync
// data of wrong type. The local version should not get modified in these cases.
TEST_F(PrefServiceSyncableTest, ModelAssociationWithDataTypeMismatch) {
  prefs_.SetString(kStringPrefName, kExampleUrl0);

  syncer::SyncDataList in;
  base::Value remote_int_value(123);
  AddToRemoteDataList(kStringPrefName, remote_int_value, &in);
  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(in, &out);
  EXPECT_THAT(out, IsEmpty());
  EXPECT_THAT(prefs_.GetString(kStringPrefName), Eq(kExampleUrl0));
}

class TestPrefModelAssociatorClient : public PrefModelAssociatorClient {
 public:
  TestPrefModelAssociatorClient()
      : syncable_prefs_database_(kSyncablePrefsDatabase) {}

  TestPrefModelAssociatorClient(const TestPrefModelAssociatorClient&) = delete;
  TestPrefModelAssociatorClient& operator=(
      const TestPrefModelAssociatorClient&) = delete;

  // PrefModelAssociatorClient implementation.
  base::Value MaybeMergePreferenceValues(
      std::string_view pref_name,
      const base::Value& local_value,
      const base::Value& server_value) const override {
    return base::Value();
  }

 private:
  ~TestPrefModelAssociatorClient() override = default;

  const SyncablePrefsDatabase& GetSyncablePrefsDatabase() const override {
    return syncable_prefs_database_;
  }

  TestSyncablePrefsDatabase syncable_prefs_database_;
};

class PrefServiceSyncableMergeTest : public testing::Test {
 public:
  PrefServiceSyncableMergeTest()
      : prefs_(
            std::unique_ptr<PrefNotifierImpl>(pref_notifier_),
            std::make_unique<PrefValueStore>(managed_prefs_.get(),
                                             new TestingPrefStore,
                                             new TestingPrefStore,
                                             new TestingPrefStore,
                                             new TestingPrefStore,
                                             user_prefs_.get(),
                                             standalone_browser_prefs_.get(),
                                             pref_registry_->defaults().get(),
                                             pref_notifier_),
            user_prefs_,
            standalone_browser_prefs_,
            pref_registry_,
            client_,
            /*read_error_callback=*/base::DoNothing(),
            /*async=*/false) {}

  void SetUp() override {
    pref_registry_->RegisterStringPref(kUnsyncedPreferenceName,
                                       kUnsyncedPreferenceDefaultValue);
    pref_registry_->RegisterStringPref(
        kStringPrefName, std::string(),
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
    pref_registry_->RegisterListPref(
        kMergeableListPrefName,
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
    pref_registry_->RegisterDictionaryPref(
        kMergeableDictPrefName,
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
    pref_registry_->RegisterStringPref(
        kDefaultCharsetPrefName, kDefaultCharsetValue,
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

    pref_sync_service_ = static_cast<PrefModelAssociator*>(
        prefs_.GetSyncableService(syncer::PREFERENCES));
    ASSERT_THAT(pref_sync_service_, NotNull());
  }

  syncer::SyncChange MakeRemoteChange(const std::string& name,
                                      base::ValueView value,
                                      SyncChange::SyncChangeType type) {
    std::string serialized;
    JSONStringValueSerializer json(&serialized);
    CHECK(json.Serialize(value));
    sync_pb::EntitySpecifics entity;
    sync_pb::PreferenceSpecifics* pref_one = entity.mutable_preference();
    pref_one->set_name(name);
    pref_one->set_value(serialized);
    return syncer::SyncChange(FROM_HERE, type,
                              syncer::SyncData::CreateRemoteData(
                                  entity, syncer::ClientTagHash::FromUnhashed(
                                              syncer::PREFERENCES, name)));
  }

  void AddToRemoteDataList(const std::string& name,
                           base::ValueView value,
                           syncer::SyncDataList* out) {
    std::string serialized;
    JSONStringValueSerializer json(&serialized);
    ASSERT_TRUE(json.Serialize(value));
    sync_pb::EntitySpecifics one;
    sync_pb::PreferenceSpecifics* pref_one = one.mutable_preference();
    pref_one->set_name(name);
    pref_one->set_value(serialized);
    out->push_back(SyncData::CreateRemoteData(
        one, syncer::ClientTagHash::FromUnhashed(syncer::PREFERENCES, name)));
  }

  void InitWithSyncDataTakeOutput(const syncer::SyncDataList& initial_data,
                                  syncer::SyncChangeList* output) {
    std::optional<syncer::ModelError> error =
        pref_sync_service_->MergeDataAndStartSyncing(
            syncer::PREFERENCES, initial_data,
            std::make_unique<TestSyncProcessorStub>(output));
    EXPECT_FALSE(error.has_value());
  }

  const base::Value& GetPreferenceValue(const std::string& name) {
    const PrefService::Preference* preference =
        prefs_.FindPreference(name.c_str());
    return *preference->GetValue();
  }

 protected:
  scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry_ =
      base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();
  // Owned by prefs_;
  const raw_ptr<PrefNotifierImpl, DanglingUntriaged> pref_notifier_ =
      new PrefNotifierImpl;
  scoped_refptr<TestingPrefStore> managed_prefs_ =
      base::MakeRefCounted<TestingPrefStore>();
  scoped_refptr<TestingPrefStore> user_prefs_ =
      base::MakeRefCounted<TestingPrefStore>();
  scoped_refptr<TestingPrefStore> standalone_browser_prefs_ =
      base::MakeRefCounted<TestingPrefStore>();
  scoped_refptr<TestPrefModelAssociatorClient> client_ =
      base::MakeRefCounted<TestPrefModelAssociatorClient>();
  PrefServiceSyncable prefs_;
  raw_ptr<PrefModelAssociator> pref_sync_service_ = nullptr;
};

TEST_F(PrefServiceSyncableMergeTest, ShouldMergeSelectedListValues) {
  {
    ScopedListPrefUpdate update(&prefs_, kMergeableListPrefName);
    update->Append(kExampleUrl0);
    update->Append(kExampleUrl1);
  }

  auto urls_to_restore =
      base::Value::List().Append(kExampleUrl1).Append(kExampleUrl2);
  syncer::SyncDataList in;
  AddToRemoteDataList(kMergeableListPrefName, urls_to_restore, &in);

  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(in, &out);

  auto expected_urls = base::Value::List()
                           .Append(kExampleUrl1)
                           .Append(kExampleUrl2)
                           .Append(kExampleUrl0);
  std::optional<base::Value> value(FindValue(kMergeableListPrefName, out));
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, expected_urls) << *value;
  EXPECT_EQ(GetPreferenceValue(kMergeableListPrefName), expected_urls);
}

// List preferences have special handling at association time due to our ability
// to merge the local and sync value. Make sure the merge logic doesn't merge
// managed preferences.
TEST_F(PrefServiceSyncableMergeTest, ManagedListPreferences) {
  // Make the list of urls to restore on startup managed.
  auto managed_value =
      base::Value::List().Append(kExampleUrl0).Append(kExampleUrl1);
  managed_prefs_->SetValue(kMergeableListPrefName,
                           base::Value(managed_value.Clone()),
                           WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);

  // Set a cloud version.
  syncer::SyncDataList in;
  auto urls_to_restore =
      base::Value::List().Append(kExampleUrl1).Append(kExampleUrl2);
  AddToRemoteDataList(kMergeableListPrefName, urls_to_restore, &in);

  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(in, &out);

  // Start sync and verify the synced value didn't get merged.
  EXPECT_FALSE(FindValue(kMergeableListPrefName, out));

  // Changing the user-controlled value should sync as usual.
  auto user_value = base::Value::List().Append("http://chromium.org");
  prefs_.SetList(kMergeableListPrefName, user_value.Clone());
  std::optional<base::Value> actual = FindValue(kMergeableListPrefName, out);
  ASSERT_TRUE(actual);
  // The user-controlled value should be synced, not the managed one!
  EXPECT_EQ(*actual, user_value);

  // An incoming sync transaction should change the user value, not the managed
  // value.
  auto sync_value = base::Value::List().Append("http://crbug.com");
  syncer::SyncChangeList list;
  list.push_back(MakeRemoteChange(kMergeableListPrefName, sync_value,
                                  SyncChange::ACTION_UPDATE));
  pref_sync_service_->ProcessSyncChanges(FROM_HERE, list);

  const base::Value* managed_prefs_result;
  ASSERT_TRUE(
      managed_prefs_->GetValue(kMergeableListPrefName, &managed_prefs_result));
  EXPECT_EQ(managed_value, *managed_prefs_result);
  // Get should return the managed value, too.
  EXPECT_EQ(managed_value, prefs_.GetValue(kMergeableListPrefName));
  // Verify the user pref value has the change.
  EXPECT_EQ(sync_value, *prefs_.GetUserPrefValue(kMergeableListPrefName));
}

TEST_F(PrefServiceSyncableMergeTest, ShouldMergeSelectedDictionaryValues) {
  {
    ScopedDictPrefUpdate update(&prefs_, kMergeableDictPrefName);
    update->Set("my_key1", "my_value1");
    update->Set("my_key3", "my_value3");
  }

  auto remote_update =
      base::Value::Dict().Set("my_key2", base::Value("my_value2"));
  syncer::SyncDataList in;
  AddToRemoteDataList(kMergeableDictPrefName, remote_update, &in);

  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(in, &out);

  auto expected_dict = base::Value::Dict()
                           .Set("my_key1", base::Value("my_value1"))
                           .Set("my_key2", base::Value("my_value2"))
                           .Set("my_key3", base::Value("my_value3"));
  std::optional<base::Value> value(FindValue(kMergeableDictPrefName, out));
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, expected_dict);
  EXPECT_EQ(GetPreferenceValue(kMergeableDictPrefName), expected_dict);
}

// TODO(jamescook): In production all prefs are registered before the
// PrefServiceSyncable is created. This test should do the same.
TEST_F(PrefServiceSyncableMergeTest, KeepPriorityPreferencesSeparately) {
  pref_registry_->RegisterStringPref(
      kBrowserPriorityPrefName, "priority-default",
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);

  syncer::SyncDataList in;
  // AddToRemoteDataList() produces sync data for non-priority prefs.
  AddToRemoteDataList(kBrowserPriorityPrefName,
                      base::Value("non-priority-value"), &in);
  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(in, &out);
  EXPECT_THAT(GetPreferenceValue(kBrowserPriorityPrefName).GetString(),
              Eq("priority-default"));
}

TEST_F(PrefServiceSyncableMergeTest, ShouldIgnoreUpdatesToNotSyncablePrefs) {
  syncer::SyncDataList in;
  AddToRemoteDataList(kUnsyncedPreferenceName, base::Value("remote_value"),
                      &in);
  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(in, &out);
  EXPECT_THAT(GetPreferenceValue(kUnsyncedPreferenceName).GetString(),
              Eq(kUnsyncedPreferenceDefaultValue));

  syncer::SyncChangeList remote_changes;
  remote_changes.push_back(MakeRemoteChange(kUnsyncedPreferenceName,
                                            base::Value("remote_value2"),
                                            SyncChange::ACTION_UPDATE));
  pref_sync_service_->ProcessSyncChanges(FROM_HERE, remote_changes);
  // The pref isn't synced.
  EXPECT_FALSE(
      pref_sync_service_->IsPrefSyncedForTesting(kUnsyncedPreferenceName));
  EXPECT_THAT(GetPreferenceValue(kUnsyncedPreferenceName).GetString(),
              Eq(kUnsyncedPreferenceDefaultValue));
}

using PrefServiceSyncableMetricsTest = PrefServiceSyncableMergeTest;

TEST_F(PrefServiceSyncableMetricsTest, RecordRemoteIncrementalChange) {
  constexpr std::string_view kHistogramName =
      "Sync.SyncablePrefIncomingIncrementalUpdate";

  InitWithSyncDataTakeOutput({}, nullptr);

  base::HistogramTester tester;

  // Remote incremental updates.
  syncer::SyncChangeList update;
  update.push_back(MakeRemoteChange(kStringPrefName,
                                    base::Value(base::Value::Type::STRING),
                                    SyncChange::ACTION_DELETE));
  update.push_back(MakeRemoteChange(kMergeableListPrefName,
                                    base::Value(base::Value::Type::LIST),
                                    SyncChange::ACTION_ADD));
  update.push_back(MakeRemoteChange(kMergeableDictPrefName,
                                    base::Value(base::Value::Type::DICT),
                                    SyncChange::ACTION_UPDATE));

  pref_sync_service_->ProcessSyncChanges(FROM_HERE, update);

  // Updates for the three syncable prefs were recorded.
  tester.ExpectTotalCount(kHistogramName, /*expected_count=*/3);
  tester.ExpectBucketCount(kHistogramName,
                           /*sample=*/1, /*expected_count=*/1);
  tester.ExpectBucketCount(kHistogramName,
                           /*sample=*/3,
                           /*expected_count=*/1);
  tester.ExpectBucketCount(kHistogramName,
                           /*sample=*/4, /*expected_count=*/1);
}

TEST_F(PrefServiceSyncableTest, FailModelAssociation) {
  syncer::SyncChangeList output;
  TestSyncProcessorStub* stub = new TestSyncProcessorStub(&output);
  stub->FailNextProcessSyncChanges();
  std::optional<syncer::ModelError> error =
      pref_sync_service_->MergeDataAndStartSyncing(
          syncer::PREFERENCES, syncer::SyncDataList(), base::WrapUnique(stub));
  EXPECT_TRUE(error.has_value());
}

TEST_F(PrefServiceSyncableTest, UpdatedPreferenceWithDefaultValue) {
  const PrefService::Preference* pref = prefs_.FindPreference(kStringPrefName);
  EXPECT_TRUE(pref->IsDefaultValue());

  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);
  out.clear();

  base::Value expected(kExampleUrl0);
  GetPrefs()->Set(kStringPrefName, expected);

  std::optional<base::Value> actual(FindValue(kStringPrefName, out));
  ASSERT_TRUE(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(PrefServiceSyncableTest, UpdatedPreferenceWithValue) {
  GetPrefs()->SetString(kStringPrefName, kExampleUrl0);
  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);
  out.clear();

  base::Value expected(kExampleUrl1);
  GetPrefs()->Set(kStringPrefName, expected);

  std::optional<base::Value> actual(FindValue(kStringPrefName, out));
  ASSERT_TRUE(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(PrefServiceSyncableTest, AddAndUpdatePreference) {
  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);

  base::Value expected_add(kExampleUrl1);
  GetPrefs()->Set(kStringPrefName, expected_add);

  // This should have resulted in an ACTION_ADD.
  std::optional<base::Value> actual_add(
      FindValue(kStringPrefName, out, syncer::SyncChange::ACTION_ADD));
  ASSERT_TRUE(actual_add);
  EXPECT_EQ(expected_add, *actual_add);

  base::Value expected_update(kExampleUrl2);
  GetPrefs()->Set(kStringPrefName, expected_update);

  // Since a synced value already existed, this time it should have resulted in
  // an ACTION_UPDATE.
  std::optional<base::Value> actual_update(
      FindValue(kStringPrefName, out, syncer::SyncChange::ACTION_UPDATE));
  ASSERT_TRUE(actual_update);
  EXPECT_EQ(expected_update, *actual_update);
}

TEST_F(PrefServiceSyncableTest, StopAndRestartSync) {
  {
    syncer::SyncChangeList out;
    InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);
    ASSERT_TRUE(out.empty());

    base::Value expected_add1(kExampleUrl1);
    GetPrefs()->Set(kStringPrefName, expected_add1);

    // This should have resulted in an ACTION_ADD.
    ASSERT_TRUE(pref_sync_service_->IsPrefSyncedForTesting(kStringPrefName));
    std::optional<base::Value> actual_add1(
        FindValue(kStringPrefName, out, syncer::SyncChange::ACTION_ADD));
    ASSERT_TRUE(actual_add1);
    EXPECT_EQ(expected_add1, *actual_add1);
  }

  // Stop sync, clear the pref, then restart sync.
  pref_sync_service_->StopSyncing(syncer::PREFERENCES);
  GetPrefs()->ClearPref(kStringPrefName);

  {
    syncer::SyncChangeList out2;
    InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out2);
    ASSERT_TRUE(out2.empty());

    // The pref should not be considered synced anymore.
    EXPECT_FALSE(pref_sync_service_->IsPrefSyncedForTesting(kStringPrefName));

    // Set a new value.
    base::Value expected_add2(kExampleUrl2);
    GetPrefs()->Set(kStringPrefName, expected_add2);

    // This should have resulted in an ACTION_ADD again.
    std::optional<base::Value> actual_add2(
        FindValue(kStringPrefName, out2, syncer::SyncChange::ACTION_ADD));
    ASSERT_TRUE(actual_add2);
    EXPECT_EQ(expected_add2, *actual_add2);
  }
}

TEST_F(PrefServiceSyncableTest, UpdatedSyncNodeActionUpdate) {
  GetPrefs()->SetString(kStringPrefName, kExampleUrl0);
  InitWithNoSyncData();

  base::Value expected(kExampleUrl1);
  syncer::SyncChangeList list;
  list.push_back(
      MakeRemoteChange(kStringPrefName, expected, SyncChange::ACTION_UPDATE));
  pref_sync_service_->ProcessSyncChanges(FROM_HERE, list);

  const base::Value& actual = GetPreferenceValue(kStringPrefName);
  EXPECT_EQ(expected, actual);
}

// Verifies that the implementation gracefully handles a remote update with the
// wrong type. The local version should not get modified in these cases.
TEST_F(PrefServiceSyncableTest, UpdatedSyncNodeActionUpdateTypeMismatch) {
  GetPrefs()->SetString(kStringPrefName, kExampleUrl0);
  InitWithNoSyncData();

  base::Value remote_int_value(123);
  syncer::SyncChangeList remote_changes;
  remote_changes.push_back(MakeRemoteChange(kStringPrefName, remote_int_value,
                                            SyncChange::ACTION_UPDATE));
  pref_sync_service_->ProcessSyncChanges(FROM_HERE, remote_changes);

  EXPECT_THAT(prefs_.GetString(kStringPrefName), Eq(kExampleUrl0));
}

TEST_F(PrefServiceSyncableTest, UpdatedSyncNodeActionAdd) {
  InitWithNoSyncData();

  base::Value expected(kExampleUrl0);
  syncer::SyncChangeList list;
  list.push_back(
      MakeRemoteChange(kStringPrefName, expected, SyncChange::ACTION_ADD));
  pref_sync_service_->ProcessSyncChanges(FROM_HERE, list);

  const base::Value& actual = GetPreferenceValue(kStringPrefName);
  EXPECT_EQ(expected, actual);
  EXPECT_TRUE(pref_sync_service_->IsPrefSyncedForTesting(kStringPrefName));
}

TEST_F(PrefServiceSyncableTest, UpdatedSyncNodeUnknownPreference) {
  InitWithNoSyncData();
  syncer::SyncChangeList list;
  base::Value expected(kExampleUrl0);
  list.push_back(MakeRemoteChange("unknown preference", expected,
                                  SyncChange::ACTION_UPDATE));
  pref_sync_service_->ProcessSyncChanges(FROM_HERE, list);
  // Nothing interesting happens on the client when it gets an update
  // of an unknown preference.  We just should not crash.
}

TEST_F(PrefServiceSyncableTest, ManagedPreferences) {
  // Make the homepage preference managed.
  base::Value managed_value("http://example.com");
  prefs_.SetManagedPref(kStringPrefName, managed_value.Clone());

  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);
  out.clear();

  // Changing the user-controlled value of the preference should still sync as
  // usual.
  base::Value user_value("http://chromium.org");
  prefs_.SetUserPref(kStringPrefName, user_value.Clone());
  std::optional<base::Value> actual = FindValue(kStringPrefName, out);
  ASSERT_TRUE(actual);
  // The user-controlled value should be synced, not the managed one!
  EXPECT_EQ(*actual, user_value);

  // An incoming sync transaction should change the user value, not the managed
  // value.
  base::Value sync_value("http://crbug.com");
  syncer::SyncChangeList list;
  list.push_back(
      MakeRemoteChange(kStringPrefName, sync_value, SyncChange::ACTION_UPDATE));
  pref_sync_service_->ProcessSyncChanges(FROM_HERE, list);

  EXPECT_EQ(managed_value, *prefs_.GetManagedPref(kStringPrefName));
  EXPECT_EQ(sync_value, *prefs_.GetUserPref(kStringPrefName));
}

TEST_F(PrefServiceSyncableTest, DynamicManagedPreferences) {
  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);
  out.clear();
  base::Value initial_value("http://example.com/initial");
  GetPrefs()->Set(kStringPrefName, initial_value);
  std::optional<base::Value> actual(FindValue(kStringPrefName, out));
  ASSERT_TRUE(actual);
  EXPECT_EQ(initial_value, *actual);

  // Switch kHomePage to managed and set a different value.
  base::Value managed_value("http://example.com/managed");
  GetTestingPrefService()->SetManagedPref(kStringPrefName,
                                          managed_value.Clone());

  // The pref value should be the one dictated by policy.
  EXPECT_EQ(managed_value, GetPreferenceValue(kStringPrefName));

  // Switch kHomePage back to unmanaged.
  GetTestingPrefService()->RemoveManagedPref(kStringPrefName);

  // The original value should be picked up.
  EXPECT_EQ(initial_value, GetPreferenceValue(kStringPrefName));
}

TEST_F(PrefServiceSyncableTest, DynamicManagedPreferencesWithSyncChange) {
  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);
  out.clear();

  base::Value initial_value("http://example.com/initial");
  GetPrefs()->Set(kStringPrefName, initial_value);
  std::optional<base::Value> actual(FindValue(kStringPrefName, out));
  EXPECT_EQ(initial_value, *actual);

  // Switch kHomePage to managed and set a different value.
  base::Value managed_value("http://example.com/managed");
  GetTestingPrefService()->SetManagedPref(kStringPrefName,
                                          managed_value.Clone());

  // Change the sync value.
  base::Value sync_value("http://example.com/sync");
  syncer::SyncChangeList list;
  list.push_back(
      MakeRemoteChange(kStringPrefName, sync_value, SyncChange::ACTION_UPDATE));
  pref_sync_service_->ProcessSyncChanges(FROM_HERE, list);

  // The pref value should still be the one dictated by policy.
  EXPECT_EQ(managed_value, GetPreferenceValue(kStringPrefName));

  // Switch kHomePage back to unmanaged.
  GetTestingPrefService()->RemoveManagedPref(kStringPrefName);

  // Sync value should be picked up.
  EXPECT_EQ(sync_value, GetPreferenceValue(kStringPrefName));
}

TEST_F(PrefServiceSyncableTest, DynamicManagedDefaultPreferences) {
  const PrefService::Preference* pref = prefs_.FindPreference(kStringPrefName);
  EXPECT_TRUE(pref->IsDefaultValue());
  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);

  EXPECT_TRUE(IsRegistered(kStringPrefName));
  EXPECT_TRUE(pref->IsDefaultValue());
  EXPECT_FALSE(FindValue(kStringPrefName, out));
  out.clear();

  // Switch kHomePage to managed and set a different value.
  base::Value managed_value("http://example.com/managed");
  GetTestingPrefService()->SetManagedPref(kStringPrefName,
                                          managed_value.Clone());
  // The pref value should be the one dictated by policy.
  EXPECT_EQ(managed_value, GetPreferenceValue(kStringPrefName));
  EXPECT_FALSE(pref->IsDefaultValue());
  // There should be no synced value.
  EXPECT_FALSE(FindValue(kStringPrefName, out));
  // Switch kHomePage back to unmanaged.
  GetTestingPrefService()->RemoveManagedPref(kStringPrefName);
  // The original value should be picked up.
  EXPECT_TRUE(pref->IsDefaultValue());
  // There should still be no synced value.
  EXPECT_FALSE(FindValue(kStringPrefName, out));
}

TEST_F(PrefServiceSyncableTest, DeletePreference) {
  prefs_.SetString(kStringPrefName, kExampleUrl0);
  const PrefService::Preference* pref = prefs_.FindPreference(kStringPrefName);
  EXPECT_FALSE(pref->IsDefaultValue());

  InitWithNoSyncData();

  base::Value null_value;
  syncer::SyncChangeList list;
  list.push_back(
      MakeRemoteChange(kStringPrefName, null_value, SyncChange::ACTION_DELETE));
  pref_sync_service_->ProcessSyncChanges(FROM_HERE, list);
  EXPECT_TRUE(pref->IsDefaultValue());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// The Chrome OS tests exercise pref model association that happens in the
// constructor of PrefServiceSyncable. The tests must register prefs first,
// then create the PrefServiceSyncable object. The tests live in this file
// because they share utility code with the cross-platform tests.
class PrefServiceSyncableChromeOsTest : public testing::Test {
 public:
  PrefServiceSyncableChromeOsTest()
      : pref_registry_(base::MakeRefCounted<PrefRegistrySyncable>()),
        pref_notifier_(new PrefNotifierImpl),
        user_prefs_(base::MakeRefCounted<TestingPrefStore>()),
        standalone_browser_prefs_(base::MakeRefCounted<TestingPrefStore>()),
        managed_prefs_(base::MakeRefCounted<TestingPrefStore>()),
        supervised_user_prefs_(base::MakeRefCounted<TestingPrefStore>()),
        extension_prefs_(base::MakeRefCounted<TestingPrefStore>()),
        command_line_prefs_(base::MakeRefCounted<TestingPrefStore>()),
        recommended_prefs_(base::MakeRefCounted<TestingPrefStore>()),
        client_(base::MakeRefCounted<TestPrefModelAssociatorClient>()) {}

  void CreatePrefService() {
    // Register prefs of various types.
    pref_registry_->RegisterStringPref(kUnsyncedPreferenceName, std::string());
    pref_registry_->RegisterStringPref(kBrowserPrefName, std::string(),
                                       PrefRegistrySyncable::SYNCABLE_PREF);
    pref_registry_->RegisterStringPref(
        kBrowserPriorityPrefName, std::string(),
        PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
    pref_registry_->RegisterStringPref(kOsPrefName, std::string(),
                                       PrefRegistrySyncable::SYNCABLE_OS_PREF);
    pref_registry_->RegisterStringPref(
        kOsPriorityPrefName, std::string(),
        PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);

    // Create the PrefServiceSyncable after prefs are registered, which is the
    // order used in production.
    prefs_ = std::make_unique<PrefServiceSyncable>(
        std::unique_ptr<PrefNotifierImpl>(pref_notifier_),
        std::make_unique<PrefValueStore>(
            managed_prefs_.get(), supervised_user_prefs_.get(),
            extension_prefs_.get(), standalone_browser_prefs_.get(),
            command_line_prefs_.get(), user_prefs_.get(),
            recommended_prefs_.get(), pref_registry_->defaults().get(),
            pref_notifier_),
        user_prefs_, standalone_browser_prefs_, pref_registry_, client_,
        /*read_error_callback=*/base::DoNothing(),
        /*async=*/false);
  }

  void InitSyncForType(DataType type) {
    syncer::SyncDataList empty_data;
    std::optional<syncer::ModelError> error =
        prefs_->GetSyncableService(type)->MergeDataAndStartSyncing(
            type, empty_data, std::make_unique<TestSyncProcessorStub>(nullptr));
    EXPECT_FALSE(error.has_value());
  }

  void InitSyncForAllTypes() {
    for (DataType type : kAllPreferenceDataTypes) {
      InitSyncForType(type);
    }
  }

  DataTypeSet GetRegisteredDataTypes(const std::string& pref_name) {
    DataTypeSet registered_types;
    for (DataType type : kAllPreferenceDataTypes) {
      if (static_cast<PrefModelAssociator*>(prefs_->GetSyncableService(type))
              ->IsPrefRegistered(pref_name)) {
        registered_types.Put(type);
      }
    }
    return registered_types;
  }

  SyncData MakeRemoteSyncData(const std::string& name,
                              base::ValueView value,
                              syncer::DataType data_type) {
    std::string serialized;
    JSONStringValueSerializer json(&serialized);
    EXPECT_TRUE(json.Serialize(value));
    sync_pb::EntitySpecifics entity;
    sync_pb::PreferenceSpecifics* pref =
        PrefModelAssociator::GetMutableSpecifics(data_type, &entity);
    pref->set_name(name);
    pref->set_value(serialized);
    return SyncData::CreateRemoteData(
        entity, syncer::ClientTagHash::FromUnhashed(data_type, name));
  }

 protected:
  scoped_refptr<PrefRegistrySyncable> pref_registry_;
  raw_ptr<PrefNotifierImpl, DanglingUntriaged>
      pref_notifier_;  // Owned by |prefs_|.
  scoped_refptr<TestingPrefStore> user_prefs_;
  scoped_refptr<TestingPrefStore> standalone_browser_prefs_;
  scoped_refptr<TestingPrefStore> managed_prefs_;
  scoped_refptr<TestingPrefStore> supervised_user_prefs_;
  scoped_refptr<TestingPrefStore> extension_prefs_;
  scoped_refptr<TestingPrefStore> command_line_prefs_;
  scoped_refptr<TestingPrefStore> recommended_prefs_;
  scoped_refptr<TestPrefModelAssociatorClient> client_;
  std::unique_ptr<PrefServiceSyncable> prefs_;
};

TEST_F(PrefServiceSyncableChromeOsTest, IsPrefRegistered) {
  CreatePrefService();
  EXPECT_TRUE(GetRegisteredDataTypes(kUnsyncedPreferenceName).empty());
  EXPECT_EQ(DataTypeSet({syncer::PREFERENCES}),
            GetRegisteredDataTypes(kBrowserPrefName));
  EXPECT_EQ(DataTypeSet({syncer::PRIORITY_PREFERENCES}),
            GetRegisteredDataTypes(kBrowserPriorityPrefName));
  EXPECT_EQ(DataTypeSet({syncer::OS_PREFERENCES}),
            GetRegisteredDataTypes(kOsPrefName));
  EXPECT_EQ(DataTypeSet({syncer::OS_PRIORITY_PREFERENCES}),
            GetRegisteredDataTypes(kOsPriorityPrefName));
}

TEST_F(PrefServiceSyncableChromeOsTest, IsSyncing) {
  CreatePrefService();
  InitSyncForType(syncer::PREFERENCES);
  EXPECT_TRUE(prefs_->IsSyncing());
  EXPECT_FALSE(prefs_->IsPrioritySyncing());
  EXPECT_FALSE(prefs_->AreOsPrefsSyncing());
  EXPECT_FALSE(prefs_->AreOsPriorityPrefsSyncing());
}

TEST_F(PrefServiceSyncableChromeOsTest, IsPrioritySyncing) {
  CreatePrefService();
  InitSyncForType(syncer::PRIORITY_PREFERENCES);
  EXPECT_FALSE(prefs_->IsSyncing());
  EXPECT_TRUE(prefs_->IsPrioritySyncing());
  EXPECT_FALSE(prefs_->AreOsPrefsSyncing());
  EXPECT_FALSE(prefs_->AreOsPriorityPrefsSyncing());
}

TEST_F(PrefServiceSyncableChromeOsTest, AreOsPrefsSyncing) {
  CreatePrefService();
  InitSyncForType(syncer::OS_PREFERENCES);
  EXPECT_FALSE(prefs_->IsSyncing());
  EXPECT_FALSE(prefs_->IsPrioritySyncing());
  EXPECT_TRUE(prefs_->AreOsPrefsSyncing());
  EXPECT_FALSE(prefs_->AreOsPriorityPrefsSyncing());
}

TEST_F(PrefServiceSyncableChromeOsTest, AreOsPriorityPrefsSyncing) {
  CreatePrefService();
  InitSyncForType(syncer::OS_PRIORITY_PREFERENCES);
  EXPECT_FALSE(prefs_->IsSyncing());
  EXPECT_FALSE(prefs_->IsPrioritySyncing());
  EXPECT_FALSE(prefs_->AreOsPrefsSyncing());
  EXPECT_TRUE(prefs_->AreOsPriorityPrefsSyncing());
}

TEST_F(PrefServiceSyncableChromeOsTest, IsPrefSynced_OsPref) {
  CreatePrefService();
  InitSyncForAllTypes();
  auto* associator = static_cast<PrefModelAssociator*>(
      prefs_->GetSyncableService(syncer::OS_PREFERENCES));
  EXPECT_FALSE(associator->IsPrefSyncedForTesting(kOsPrefName));

  syncer::SyncChangeList list;
  list.push_back(MakeRemoteChange(kOsPrefName, base::Value("value"),
                                  SyncChange::ACTION_ADD,
                                  syncer::OS_PREFERENCES));
  associator->ProcessSyncChanges(FROM_HERE, list);
  EXPECT_TRUE(associator->IsPrefSyncedForTesting(kOsPrefName));
}

TEST_F(PrefServiceSyncableChromeOsTest, IsPrefSynced_OsPriorityPref) {
  CreatePrefService();
  InitSyncForAllTypes();
  auto* associator = static_cast<PrefModelAssociator*>(
      prefs_->GetSyncableService(syncer::OS_PRIORITY_PREFERENCES));
  EXPECT_FALSE(associator->IsPrefSyncedForTesting(kOsPriorityPrefName));

  syncer::SyncChangeList list;
  list.push_back(MakeRemoteChange(kOsPriorityPrefName, base::Value("value"),
                                  SyncChange::ACTION_ADD,
                                  syncer::OS_PRIORITY_PREFERENCES));
  associator->ProcessSyncChanges(FROM_HERE, list);
  EXPECT_TRUE(associator->IsPrefSyncedForTesting(kOsPriorityPrefName));
}

TEST_F(PrefServiceSyncableChromeOsTest, SyncedPrefObserver_OsPref) {
  CreatePrefService();
  InitSyncForAllTypes();

  TestSyncedPrefObserver observer;
  prefs_->AddSyncedPrefObserver(kOsPrefName, &observer);

  prefs_->SetString(kOsPrefName, "value");
  EXPECT_EQ(kOsPrefName, observer.last_pref_);
  EXPECT_EQ(1, observer.changed_count_);

  prefs_->RemoveSyncedPrefObserver(kOsPrefName, &observer);
}

TEST_F(PrefServiceSyncableChromeOsTest, SyncedPrefObserver_OsPriorityPref) {
  CreatePrefService();
  InitSyncForAllTypes();

  TestSyncedPrefObserver observer;
  prefs_->AddSyncedPrefObserver(kOsPriorityPrefName, &observer);

  prefs_->SetString(kOsPriorityPrefName, "value");
  EXPECT_EQ(kOsPriorityPrefName, observer.last_pref_);
  EXPECT_EQ(1, observer.changed_count_);

  prefs_->RemoveSyncedPrefObserver(kOsPriorityPrefName, &observer);
}

TEST_F(PrefServiceSyncableChromeOsTest,
       UpdatesFromOldClientsAreIgnored_Startup) {
  CreatePrefService();
  TestSyncedPrefObserver observer;
  prefs_->AddSyncedPrefObserver(kOsPrefName, &observer);

  // Simulate an old client that has `kOsPrefName` registered as SYNCABLE_PREF
  // instead of SYNCABLE_OS_PREF.
  syncer::SyncDataList list;
  list.push_back(CreateRemoteSyncData(kOsPrefName, base::Value("new_value")));

  // Simulate the first sync at startup of the legacy browser prefs DataType.
  auto* browser_associator = static_cast<PrefModelAssociator*>(
      prefs_->GetSyncableService(syncer::PREFERENCES));
  syncer::SyncChangeList outgoing_changes;
  browser_associator->MergeDataAndStartSyncing(
      syncer::PREFERENCES, list,
      std::make_unique<TestSyncProcessorStub>(&outgoing_changes));

  // No outgoing changes were triggered.
  EXPECT_TRUE(outgoing_changes.empty());

  // The value from the old client was not applied.
  EXPECT_NE("new_value", prefs_->GetString(kOsPrefName));

  // The pref is not considered to be syncing, because it still has its default
  // value.
  EXPECT_FALSE(browser_associator->IsPrefSyncedForTesting(kOsPrefName));

  // Observers were not notified of changes.
  EXPECT_EQ(0, observer.changed_count_);

  prefs_->RemoveSyncedPrefObserver(kOsPrefName, &observer);
}

TEST_F(PrefServiceSyncableChromeOsTest,
       UpdatesFromOldClientsAreIgnored_Update) {
  CreatePrefService();
  InitSyncForAllTypes();
  TestSyncedPrefObserver observer;
  prefs_->AddSyncedPrefObserver(kOsPrefName, &observer);

  syncer::SyncChangeList list;
  // Simulate an old client that has `kOsPrefName` registered as SYNCABLE_PREF
  // instead of SYNCABLE_OS_PREF.
  list.push_back(MakeRemoteChange(kOsPrefName, base::Value("new_value"),
                                  SyncChange::ACTION_ADD, syncer::PREFERENCES));

  // Simulate a sync update after startup.
  prefs_->GetSyncableService(syncer::PREFERENCES)
      ->ProcessSyncChanges(FROM_HERE, list);

  // Update was not applied.
  EXPECT_NE("new_value", prefs_->GetString(kOsPrefName));

  // Observers were not notified of changes.
  EXPECT_EQ(0, observer.changed_count_);

  prefs_->RemoveSyncedPrefObserver(kOsPrefName, &observer);
}

TEST_F(PrefServiceSyncableChromeOsTest,
       SyncedPrefObserver_OsPrefIsChangedFromSync) {
  CreatePrefService();
  prefs_->SetString(kOsPrefName, "default_value");

  TestSyncedPrefObserver observer;
  prefs_->AddSyncedPrefObserver(kOsPrefName, &observer);

  TestPrefServiceSyncableObserver pref_service_sync_observer;
  pref_service_sync_observer.SetSyncedPrefObserver(&observer);
  prefs_->AddObserver(&pref_service_sync_observer);

  // Simulate that `kOsPrefName` is registered as SYNCABLE_PREF
  syncer::SyncDataList list;
  list.push_back(MakeRemoteSyncData(kOsPrefName, base::Value("new_value"),
                                    syncer::OS_PREFERENCES));

  // Simulate the first sync at startup.
  syncer::SyncChangeList outgoing_changes;
  prefs_->GetSyncableService(syncer::OS_PREFERENCES)
      ->MergeDataAndStartSyncing(
          syncer::OS_PREFERENCES, list,
          std::make_unique<TestSyncProcessorStub>(&outgoing_changes));

  EXPECT_EQ(kOsPrefName, observer.synced_pref_);
  EXPECT_EQ(1, observer.sync_started_count_);
  EXPECT_TRUE(pref_service_sync_observer.is_syncing_changed());

  prefs_->RemoveObserver(&pref_service_sync_observer);
  prefs_->RemoveSyncedPrefObserver(kOsPrefName, &observer);
}

TEST_F(PrefServiceSyncableChromeOsTest,
       SyncedPrefObserver_OsPrefIsNotChangedFromSync) {
  CreatePrefService();
  prefs_->SetString(kOsPrefName, "default_value");

  TestSyncedPrefObserver observer;
  prefs_->AddSyncedPrefObserver(kOsPrefName, &observer);

  TestPrefServiceSyncableObserver pref_service_sync_observer;
  pref_service_sync_observer.SetSyncedPrefObserver(&observer);
  prefs_->AddObserver(&pref_service_sync_observer);

  // Simulate that `kOsPrefName` is registered as SYNCABLE_PREF
  syncer::SyncDataList list;
  list.push_back(MakeRemoteSyncData(kOsPrefName, base::Value("new_value"),
                                    syncer::OS_PREFERENCES));

  // Simulate the first sync at startup.
  syncer::SyncChangeList outgoing_changes;
  prefs_->GetSyncableService(syncer::OS_PREFERENCES)
      ->MergeDataAndStartSyncing(
          syncer::OS_PREFERENCES, list,
          std::make_unique<TestSyncProcessorStub>(&outgoing_changes));

  EXPECT_EQ(kOsPrefName, observer.synced_pref_);
  EXPECT_EQ(1, observer.sync_started_count_);
  EXPECT_TRUE(pref_service_sync_observer.is_syncing_changed());

  prefs_->RemoveObserver(&pref_service_sync_observer);
  prefs_->RemoveSyncedPrefObserver(kOsPrefName, &observer);
}

TEST_F(PrefServiceSyncableChromeOsTest, SyncedPrefObserver_EmptyCloud) {
  CreatePrefService();
  prefs_->SetString(kOsPrefName, "new_value");

  TestSyncedPrefObserver observer;
  prefs_->AddSyncedPrefObserver(kOsPrefName, &observer);

  // Simulate the first sync at startup.
  syncer::SyncChangeList outgoing_changes;
  prefs_->GetSyncableService(syncer::OS_PREFERENCES)
      ->MergeDataAndStartSyncing(
          syncer::OS_PREFERENCES, syncer::SyncDataList(),
          std::make_unique<TestSyncProcessorStub>(&outgoing_changes));

  EXPECT_EQ("", observer.synced_pref_);
  EXPECT_EQ(0, observer.sync_started_count_);

  prefs_->RemoveSyncedPrefObserver(kOsPrefName, &observer);
}

TEST_F(PrefServiceSyncableChromeOsTest,
       StandaloneBrowserPrefsNotLeakedInIncognito) {
  CreatePrefService();

  prefs_->SetStandaloneBrowserPref(kOsPrefName, base::Value("test_value"));

  scoped_refptr<TestingPrefStore> incognito_extension_pref_store =
      base::MakeRefCounted<TestingPrefStore>();

  std::unique_ptr<PrefServiceSyncable> incognito_prefs =
      prefs_->CreateIncognitoPrefService(incognito_extension_pref_store.get(),
                                         /*persistent_pref_names=*/{});

  // Verify that the primary profile has the `kOsPrefName` pref set.
  {
    const PrefService::Preference* main_profile_pref =
        prefs_->FindPreference(kOsPrefName);
    ASSERT_TRUE(main_profile_pref);
    EXPECT_TRUE(main_profile_pref->IsStandaloneBrowserControlled());
    EXPECT_EQ(*main_profile_pref->GetValue(), base::Value("test_value"));
  }

  // Verify that the incognito profile does not have the `kOsPrefName` pref set.
  {
    const PrefService::Preference* incognito_pref =
        incognito_prefs->FindPreference(kOsPrefName);
    ASSERT_TRUE(incognito_pref);
    EXPECT_FALSE(incognito_pref->IsStandaloneBrowserControlled());
    EXPECT_EQ(*incognito_pref->GetValue(), base::Value(""));
  }

  // Ensure this does not crash if it's accidentally called.
  incognito_prefs->SetStandaloneBrowserPref(kOsPrefName,
                                            base::Value("test_value"));
  // Verify that standalone browser settings cannot be configured by the
  // Incognito profile.
  {
    const PrefService::Preference* incognito_pref =
        incognito_prefs->FindPreference(kOsPrefName);
    ASSERT_TRUE(incognito_pref);
    EXPECT_FALSE(incognito_pref->IsStandaloneBrowserControlled());
    EXPECT_EQ(*incognito_pref->GetValue(), base::Value(""));
  }
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class PrefServiceSyncableFactoryTest : public PrefServiceSyncableTest {
 public:
  PrefServiceSyncableFactoryTest() {
    pref_service_syncable_factory_.set_user_prefs(user_prefs_);
    pref_service_syncable_factory_.SetAccountPrefStore(account_prefs_);
  }

 protected:
  PrefServiceSyncableFactory pref_service_syncable_factory_;
  scoped_refptr<TestingPrefStore> user_prefs_ =
      base::MakeRefCounted<TestingPrefStore>();
  scoped_refptr<TestingPrefStore> account_prefs_ =
      base::MakeRefCounted<TestingPrefStore>();
};

TEST_F(PrefServiceSyncableFactoryTest,
       ShouldCreateSyncServiceWithoutDualLayerStoreIfFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(syncer::kEnablePreferencesAccountStorage);
  auto pref_service =
      pref_service_syncable_factory_.CreateSyncable(prefs_.registry());
  EXPECT_FALSE(static_cast<PrefModelAssociator*>(
                   pref_service->GetSyncableService(syncer::PREFERENCES))
                   ->IsUsingDualLayerUserPrefStoreForTesting());
  EXPECT_FALSE(
      static_cast<PrefModelAssociator*>(
          pref_service->GetSyncableService(syncer::PRIORITY_PREFERENCES))
          ->IsUsingDualLayerUserPrefStoreForTesting());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_FALSE(static_cast<PrefModelAssociator*>(
                   pref_service->GetSyncableService(syncer::OS_PREFERENCES))
                   ->IsUsingDualLayerUserPrefStoreForTesting());
  EXPECT_FALSE(
      static_cast<PrefModelAssociator*>(
          pref_service->GetSyncableService(syncer::OS_PRIORITY_PREFERENCES))
          ->IsUsingDualLayerUserPrefStoreForTesting());
#endif
}

TEST_F(PrefServiceSyncableFactoryTest,
       ShouldCreateSyncServiceWithDualLayerStoreIfFeatureEnabled) {
  base::test::ScopedFeatureList feature_list(
      syncer::kEnablePreferencesAccountStorage);
  auto pref_service =
      pref_service_syncable_factory_.CreateSyncable(prefs_.registry());
  EXPECT_TRUE(static_cast<PrefModelAssociator*>(
                  pref_service->GetSyncableService(syncer::PREFERENCES))
                  ->IsUsingDualLayerUserPrefStoreForTesting());
  EXPECT_TRUE(
      static_cast<PrefModelAssociator*>(
          pref_service->GetSyncableService(syncer::PRIORITY_PREFERENCES))
          ->IsUsingDualLayerUserPrefStoreForTesting());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_TRUE(static_cast<PrefModelAssociator*>(
                  pref_service->GetSyncableService(syncer::OS_PREFERENCES))
                  ->IsUsingDualLayerUserPrefStoreForTesting());
  EXPECT_TRUE(
      static_cast<PrefModelAssociator*>(
          pref_service->GetSyncableService(syncer::OS_PRIORITY_PREFERENCES))
          ->IsUsingDualLayerUserPrefStoreForTesting());
#endif
}

}  // namespace

}  // namespace sync_preferences
