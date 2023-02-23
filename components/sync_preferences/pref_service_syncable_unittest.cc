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
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_store.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/preference_specifics.pb.h"
#include "components/sync_preferences/pref_model_associator.h"
#include "components/sync_preferences/pref_model_associator_client.h"
#include "components/sync_preferences/pref_service_syncable_observer.h"
#include "components/sync_preferences/syncable_prefs_database.h"
#include "components/sync_preferences/synced_pref_observer.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "testing/gmock/include/gmock/gmock-matchers.h"
#endif

using syncer::ModelType;
using syncer::ModelTypeSet;
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
const char kDictPrefName[] = "dict_pref_name";
const char kUnsyncedPreferenceName[] = "nonsense_pref_name";
const char kUnsyncedPreferenceDefaultValue[] = "default";
const char kDefaultCharsetPrefName[] = "default_charset";
const char kNonDefaultCharsetValue[] = "foo";
const char kDefaultCharsetValue[] = "utf-8";

// Searches for a preference matching `name` and, if specified,`change_type`,
// within `list`. Returns the value of the first matching pref, or nullopt if
// none is found.
absl::optional<base::Value> FindValue(
    const std::string& name,
    const syncer::SyncChangeList& list,
    absl::optional<syncer::SyncChange::SyncChangeType> change_type =
        absl::nullopt) {
  for (const SyncChange& change : list) {
    if ((!change_type || change.change_type() == *change_type) &&
        change.sync_data().GetClientTagHash() ==
            syncer::ClientTagHash::FromUnhashed(syncer::PREFERENCES, name)) {
      return base::JSONReader::Read(
          change.sync_data().GetSpecifics().preference().value());
    }
  }
  return absl::nullopt;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr ModelTypeSet kAllPreferenceModelTypes(
    syncer::PREFERENCES,
    syncer::PRIORITY_PREFERENCES,
    syncer::OS_PREFERENCES,
    syncer::OS_PRIORITY_PREFERENCES);

MATCHER_P(MatchesModelType, model_type, "") {
  const syncer::SyncChange& sync_change = arg;
  return Matches(model_type)(sync_change.sync_data().GetDataType());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class TestSyncProcessorStub : public syncer::SyncChangeProcessor {
 public:
  explicit TestSyncProcessorStub(syncer::SyncChangeList* output)
      : output_(output) {}

  absl::optional<syncer::ModelError> ProcessSyncChanges(
      const base::Location& from_here,
      const syncer::SyncChangeList& change_list) override {
    if (output_) {
      output_->insert(output_->end(), change_list.begin(), change_list.end());
    }
    if (fail_next_) {
      fail_next_ = false;
      return syncer::ModelError(FROM_HERE, "Error");
    }
    return absl::nullopt;
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

  void OnSyncedPrefChanged(const std::string& path, bool from_sync) override {
    last_pref_ = path;
    changed_count_++;
  }

  void OnStartedSyncing(const std::string& path) override {
    synced_pref_ = path;
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
                                    syncer::ModelType model_type) {
  std::string serialized;
  JSONStringValueSerializer json(&serialized);
  bool success = json.Serialize(value);
  DCHECK(success);
  sync_pb::EntitySpecifics entity;
  sync_pb::PreferenceSpecifics* pref =
      PrefModelAssociator::GetMutableSpecifics(model_type, &entity);
  pref->set_name(name);
  pref->set_value(serialized);
  return syncer::SyncChange(
      FROM_HERE, change_type,
      syncer::SyncData::CreateRemoteData(
          entity, syncer::ClientTagHash::FromUnhashed(model_type, name)));
}

// Creates a SyncChange for model type |PREFERENCES|.
syncer::SyncChange MakeRemoteChange(const std::string& name,
                                    base::ValueView value,
                                    SyncChange::SyncChangeType type) {
  return MakeRemoteChange(name, value, type, syncer::ModelType::PREFERENCES);
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
      one, syncer::ClientTagHash::FromUnhashed(syncer::ModelType::PREFERENCES,
                                               name));
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
    absl::optional<syncer::ModelError> error =
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

  absl::optional<base::Value> value = base::JSONReader::Read(specifics.value());
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

  absl::optional<base::Value> value(FindValue(kStringPrefName, out));
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
  base::Value::List urls_to_restore;
  urls_to_restore.Append(kExampleUrl1);
  AddToRemoteDataList(kListPrefName, urls_to_restore, &in);
  AddToRemoteDataList(kDefaultCharsetPrefName,
                      base::Value(kNonDefaultCharsetValue), &in);
  InitWithSyncDataTakeOutput(in, &out);

  ASSERT_FALSE(FindValue(kStringPrefName, out));
  ASSERT_FALSE(FindValue(kDefaultCharsetPrefName, out));

  EXPECT_EQ(kExampleUrl1, prefs_.GetString(kStringPrefName));

  // No associator client is registered, so lists and dictionaries should not
  // get merged (remote write wins).
  base::Value::List expected_urls;
  expected_urls.Append(kExampleUrl1);
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

class TestSyncablePrefsDatabase : public SyncablePrefsDatabase {
 public:
  bool IsPreferenceSyncable(const std::string& pref_name) const override {
    return true;
  }
};

class TestPrefModelAssociatorClient : public PrefModelAssociatorClient {
 public:
  TestPrefModelAssociatorClient() = default;

  TestPrefModelAssociatorClient(const TestPrefModelAssociatorClient&) = delete;
  TestPrefModelAssociatorClient& operator=(
      const TestPrefModelAssociatorClient&) = delete;

  ~TestPrefModelAssociatorClient() override = default;

  // PrefModelAssociatorClient implementation.
  bool IsMergeableListPreference(const std::string& pref_name) const override {
    return pref_name == kListPrefName;
  }

  bool IsMergeableDictionaryPreference(
      const std::string& pref_name) const override {
    return is_dict_pref_;
  }

  base::Value MaybeMergePreferenceValues(
      const std::string& pref_name,
      const base::Value& local_value,
      const base::Value& server_value) const override {
    return base::Value();
  }

  void SetIsDictPref(bool is_dict_pref) { is_dict_pref_ = is_dict_pref; }

 private:
  const SyncablePrefsDatabase& GetSyncablePrefsDatabase() const override {
    return syncable_prefs_database_;
  }

  TestSyncablePrefsDatabase syncable_prefs_database_;

  bool is_dict_pref_ = true;
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
            &client_,
            /*read_error_callback=*/base::DoNothing(),
            /*async=*/false) {}

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
    absl::optional<syncer::ModelError> error =
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
  const raw_ptr<PrefNotifierImpl> pref_notifier_ = new PrefNotifierImpl;
  scoped_refptr<TestingPrefStore> managed_prefs_ =
      base::MakeRefCounted<TestingPrefStore>();
  scoped_refptr<TestingPrefStore> user_prefs_ =
      base::MakeRefCounted<TestingPrefStore>();
  scoped_refptr<TestingPrefStore> standalone_browser_prefs_ =
      base::MakeRefCounted<TestingPrefStore>();
  TestPrefModelAssociatorClient client_;
  PrefServiceSyncable prefs_;
  raw_ptr<PrefModelAssociator> pref_sync_service_ = nullptr;
};

TEST_F(PrefServiceSyncableMergeTest, ShouldMergeSelectedListValues) {
  {
    ScopedListPrefUpdate update(&prefs_, kListPrefName);
    update->Append(kExampleUrl0);
    update->Append(kExampleUrl1);
  }

  base::Value::List urls_to_restore;
  urls_to_restore.Append(kExampleUrl1);
  urls_to_restore.Append(kExampleUrl2);
  syncer::SyncDataList in;
  AddToRemoteDataList(kListPrefName, urls_to_restore, &in);

  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(in, &out);

  base::Value::List expected_urls;
  expected_urls.Append(kExampleUrl1);
  expected_urls.Append(kExampleUrl2);
  expected_urls.Append(kExampleUrl0);
  absl::optional<base::Value> value(FindValue(kListPrefName, out));
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, expected_urls) << *value;
  EXPECT_EQ(GetPreferenceValue(kListPrefName), expected_urls);
}

// List preferences have special handling at association time due to our ability
// to merge the local and sync value. Make sure the merge logic doesn't merge
// managed preferences.
TEST_F(PrefServiceSyncableMergeTest, ManagedListPreferences) {
  // Make the list of urls to restore on startup managed.
  base::Value::List managed_value;
  managed_value.Append(kExampleUrl0);
  managed_value.Append(kExampleUrl1);
  managed_prefs_->SetValue(kListPrefName, base::Value(managed_value.Clone()),
                           WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);

  // Set a cloud version.
  syncer::SyncDataList in;
  base::Value::List urls_to_restore;
  urls_to_restore.Append(kExampleUrl1);
  urls_to_restore.Append(kExampleUrl2);
  AddToRemoteDataList(kListPrefName, urls_to_restore, &in);

  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(in, &out);

  // Start sync and verify the synced value didn't get merged.
  EXPECT_FALSE(FindValue(kListPrefName, out));

  // Changing the user-controlled value should sync as usual.
  base::Value::List user_value;
  user_value.Append("http://chromium.org");
  prefs_.SetList(kListPrefName, user_value.Clone());
  absl::optional<base::Value> actual = FindValue(kListPrefName, out);
  ASSERT_TRUE(actual);
  // The user-controlled value should be synced, not the managed one!
  EXPECT_EQ(*actual, user_value);

  // An incoming sync transaction should change the user value, not the managed
  // value.
  base::Value::List sync_value;
  sync_value.Append("http://crbug.com");
  syncer::SyncChangeList list;
  list.push_back(
      MakeRemoteChange(kListPrefName, sync_value, SyncChange::ACTION_UPDATE));
  pref_sync_service_->ProcessSyncChanges(FROM_HERE, list);

  const base::Value* managed_prefs_result;
  ASSERT_TRUE(managed_prefs_->GetValue(kListPrefName, &managed_prefs_result));
  EXPECT_EQ(managed_value, *managed_prefs_result);
  // Get should return the managed value, too.
  EXPECT_EQ(managed_value, prefs_.GetValue(kListPrefName));
  // Verify the user pref value has the change.
  EXPECT_EQ(sync_value, *prefs_.GetUserPrefValue(kListPrefName));
}

TEST_F(PrefServiceSyncableMergeTest, ShouldMergeSelectedDictionaryValues) {
  {
    ScopedDictPrefUpdate update(&prefs_, kDictPrefName);
    update->Set("my_key1", "my_value1");
    update->Set("my_key3", "my_value3");
  }

  base::Value::Dict remote_update;
  remote_update.Set("my_key2", base::Value("my_value2"));
  syncer::SyncDataList in;
  AddToRemoteDataList(kDictPrefName, remote_update, &in);

  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(in, &out);

  base::Value::Dict expected_dict;
  expected_dict.Set("my_key1", base::Value("my_value1"));
  expected_dict.Set("my_key2", base::Value("my_value2"));
  expected_dict.Set("my_key3", base::Value("my_value3"));
  absl::optional<base::Value> value(FindValue(kDictPrefName, out));
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, expected_dict);
  EXPECT_EQ(GetPreferenceValue(kDictPrefName), expected_dict);
}

// TODO(jamescook): In production all prefs are registered before the
// PrefServiceSyncable is created. This test should do the same.
TEST_F(PrefServiceSyncableMergeTest, KeepPriorityPreferencesSeparately) {
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
      pref_name, base::Value("remote_value2"), SyncChange::ACTION_UPDATE));
  pref_sync_service_->ProcessSyncChanges(FROM_HERE, remote_changes);
  // The pref isn't synced.
  EXPECT_FALSE(pref_sync_service_->IsPrefSyncedForTesting(pref_name));
  EXPECT_THAT(GetPreferenceValue(pref_name).GetString(), Eq("default_value"));
}

TEST_F(PrefServiceSyncableTest, FailModelAssociation) {
  syncer::SyncChangeList output;
  TestSyncProcessorStub* stub = new TestSyncProcessorStub(&output);
  stub->FailNextProcessSyncChanges();
  absl::optional<syncer::ModelError> error =
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

  absl::optional<base::Value> actual(FindValue(kStringPrefName, out));
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

  absl::optional<base::Value> actual(FindValue(kStringPrefName, out));
  ASSERT_TRUE(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(PrefServiceSyncableTest, AddAndUpdatePreference) {
  syncer::SyncChangeList out;
  InitWithSyncDataTakeOutput(syncer::SyncDataList(), &out);

  base::Value expected_add(kExampleUrl1);
  GetPrefs()->Set(kStringPrefName, expected_add);

  // This should have resulted in an ACTION_ADD.
  absl::optional<base::Value> actual_add(
      FindValue(kStringPrefName, out, syncer::SyncChange::ACTION_ADD));
  ASSERT_TRUE(actual_add);
  EXPECT_EQ(expected_add, *actual_add);

  base::Value expected_update(kExampleUrl2);
  GetPrefs()->Set(kStringPrefName, expected_update);

  // Since a synced value already existed, this time it should have resulted in
  // an ACTION_UPDATE.
  absl::optional<base::Value> actual_update(
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
    absl::optional<base::Value> actual_add1(
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
    absl::optional<base::Value> actual_add2(
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
  absl::optional<base::Value> actual = FindValue(kStringPrefName, out);
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
  absl::optional<base::Value> actual(FindValue(kStringPrefName, out));
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
  absl::optional<base::Value> actual(FindValue(kStringPrefName, out));
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
        standalone_browser_prefs_(base::MakeRefCounted<TestingPrefStore>()) {}

  void CreatePrefService() {
    // Register prefs of various types.
    pref_registry_->RegisterStringPref("unsynced_pref", std::string());
    pref_registry_->RegisterStringPref("browser_pref", std::string(),
                                       PrefRegistrySyncable::SYNCABLE_PREF);
    pref_registry_->RegisterStringPref(
        "browser_priority_pref", std::string(),
        PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
    pref_registry_->RegisterStringPref("os_pref", std::string(),
                                       PrefRegistrySyncable::SYNCABLE_OS_PREF);
    pref_registry_->RegisterStringPref(
        "os_priority_pref", std::string(),
        PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
    client_.SetIsDictPref(false);

    // Create the PrefServiceSyncable after prefs are registered, which is the
    // order used in production.
    prefs_ = std::make_unique<PrefServiceSyncable>(
        std::unique_ptr<PrefNotifierImpl>(pref_notifier_),
        std::make_unique<PrefValueStore>(
            new TestingPrefStore, new TestingPrefStore, new TestingPrefStore,
            new TestingPrefStore, new TestingPrefStore, user_prefs_.get(),
            standalone_browser_prefs_.get(), pref_registry_->defaults().get(),
            pref_notifier_),
        user_prefs_, standalone_browser_prefs_, pref_registry_, &client_,
        /*read_error_callback=*/base::DoNothing(),
        /*async=*/false);
  }

  void InitSyncForType(ModelType type,
                       syncer::SyncChangeList* output = nullptr) {
    syncer::SyncDataList empty_data;
    absl::optional<syncer::ModelError> error =
        prefs_->GetSyncableService(type)->MergeDataAndStartSyncing(
            type, empty_data, std::make_unique<TestSyncProcessorStub>(output));
    EXPECT_FALSE(error.has_value());
  }

  void InitSyncForAllTypes(syncer::SyncChangeList* output = nullptr) {
    for (ModelType type : kAllPreferenceModelTypes) {
      InitSyncForType(type, output);
    }
  }

  ModelTypeSet GetRegisteredModelTypes(const std::string& pref_name) {
    ModelTypeSet registered_types;
    for (ModelType type : kAllPreferenceModelTypes) {
      if (static_cast<PrefModelAssociator*>(prefs_->GetSyncableService(type))
              ->IsPrefRegistered(pref_name)) {
        registered_types.Put(type);
      }
    }
    return registered_types;
  }

  SyncData MakeRemoteSyncData(const std::string& name,
                              base::ValueView value,
                              syncer::ModelType model_type) {
    std::string serialized;
    JSONStringValueSerializer json(&serialized);
    EXPECT_TRUE(json.Serialize(value));
    sync_pb::EntitySpecifics entity;
    sync_pb::PreferenceSpecifics* pref =
        PrefModelAssociator::GetMutableSpecifics(model_type, &entity);
    pref->set_name(name);
    pref->set_value(serialized);
    return SyncData::CreateRemoteData(
        entity, syncer::ClientTagHash::FromUnhashed(model_type, name));
  }

 protected:
  scoped_refptr<PrefRegistrySyncable> pref_registry_;
  PrefNotifierImpl* pref_notifier_;  // Owned by |prefs_|.
  scoped_refptr<TestingPrefStore> user_prefs_;
  scoped_refptr<TestingPrefStore> standalone_browser_prefs_;
  TestPrefModelAssociatorClient client_;
  std::unique_ptr<PrefServiceSyncable> prefs_;
};

TEST_F(PrefServiceSyncableChromeOsTest, IsPrefRegistered) {
  CreatePrefService();
  EXPECT_TRUE(GetRegisteredModelTypes("unsynced_pref").Empty());
  EXPECT_EQ(ModelTypeSet(syncer::PREFERENCES),
            GetRegisteredModelTypes("browser_pref"));
  EXPECT_EQ(ModelTypeSet(syncer::PRIORITY_PREFERENCES),
            GetRegisteredModelTypes("browser_priority_pref"));
  EXPECT_EQ(ModelTypeSet(syncer::OS_PREFERENCES),
            GetRegisteredModelTypes("os_pref"));
  EXPECT_EQ(ModelTypeSet(syncer::OS_PRIORITY_PREFERENCES),
            GetRegisteredModelTypes("os_priority_pref"));

  // The associator for PREFERENCES knows about OS prefs so that local updates
  // are synced back to old clients.
  auto* pref_associator = static_cast<PrefModelAssociator*>(
      prefs_->GetSyncableService(syncer::PREFERENCES));
  EXPECT_TRUE(pref_associator->IsLegacyModelTypePref("os_pref"));

  // The associator for PRIORITY_PREFERENCES knows about OS priority prefs so
  // that local updates are synced back to old clients.
  auto* priority_associator = static_cast<PrefModelAssociator*>(
      prefs_->GetSyncableService(syncer::PRIORITY_PREFERENCES));
  EXPECT_TRUE(priority_associator->IsLegacyModelTypePref("os_priority_pref"));
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
  EXPECT_FALSE(associator->IsPrefSyncedForTesting("os_pref"));

  syncer::SyncChangeList list;
  list.push_back(MakeRemoteChange("os_pref", base::Value("value"),
                                  SyncChange::ACTION_ADD,
                                  syncer::OS_PREFERENCES));
  associator->ProcessSyncChanges(FROM_HERE, list);
  EXPECT_TRUE(associator->IsPrefSyncedForTesting("os_pref"));
}

TEST_F(PrefServiceSyncableChromeOsTest, IsPrefSynced_OsPriorityPref) {
  CreatePrefService();
  InitSyncForAllTypes();
  auto* associator = static_cast<PrefModelAssociator*>(
      prefs_->GetSyncableService(syncer::OS_PRIORITY_PREFERENCES));
  EXPECT_FALSE(associator->IsPrefSyncedForTesting("os_priority_pref"));

  syncer::SyncChangeList list;
  list.push_back(MakeRemoteChange("os_priority_pref", base::Value("value"),
                                  SyncChange::ACTION_ADD,
                                  syncer::OS_PRIORITY_PREFERENCES));
  associator->ProcessSyncChanges(FROM_HERE, list);
  EXPECT_TRUE(associator->IsPrefSyncedForTesting("os_priority_pref"));
}

TEST_F(PrefServiceSyncableChromeOsTest, SyncedPrefObserver_OsPref) {
  CreatePrefService();
  InitSyncForAllTypes();

  TestSyncedPrefObserver observer;
  prefs_->AddSyncedPrefObserver("os_pref", &observer);

  prefs_->SetString("os_pref", "value");
  EXPECT_EQ("os_pref", observer.last_pref_);
  EXPECT_EQ(1, observer.changed_count_);

  prefs_->RemoveSyncedPrefObserver("os_pref", &observer);
}

TEST_F(PrefServiceSyncableChromeOsTest, SyncedPrefObserver_OsPriorityPref) {
  CreatePrefService();
  InitSyncForAllTypes();

  TestSyncedPrefObserver observer;
  prefs_->AddSyncedPrefObserver("os_priority_pref", &observer);

  prefs_->SetString("os_priority_pref", "value");
  EXPECT_EQ("os_priority_pref", observer.last_pref_);
  EXPECT_EQ(1, observer.changed_count_);

  prefs_->RemoveSyncedPrefObserver("os_priority_pref", &observer);
}

TEST_F(PrefServiceSyncableChromeOsTest, OsPrefChangeSyncedAsOsPrefChange) {
  CreatePrefService();
  // Set a non-default value.
  prefs_->SetString("os_pref", "new_value");
  // Start syncing.
  syncer::SyncChangeList output;
  InitSyncForAllTypes(&output);
  ASSERT_EQ(1u, output.size());
  // The OS pref is treated like an OS pref.
  EXPECT_EQ(syncer::OS_PREFERENCES, output[0].sync_data().GetDataType());

  // Future changes will be synced back to browser preferences as well.
  auto* associator = static_cast<PrefModelAssociator*>(
      prefs_->GetSyncableService(syncer::PREFERENCES));
  EXPECT_TRUE(associator->IsPrefSyncedForTesting("os_pref"));
}

TEST_F(PrefServiceSyncableChromeOsTest,
       OsPrefChangeMakesSyncChangeForOldClients_Update) {
  CreatePrefService();
  syncer::SyncChangeList changes;
  InitSyncForAllTypes(&changes);
  EXPECT_THAT(changes, IsEmpty());

  // Make a local change.
  prefs_->SetString("os_pref", "new_value");

  // Sync changes are made for the legacy ModelType::PREFERENCES (so old clients
  // will get updates) and for the current ModelType::OS_PREFERENCES (so new
  // clients will get updates).
  EXPECT_THAT(changes,
              UnorderedElementsAre(MatchesModelType(syncer::PREFERENCES),
                                   MatchesModelType(syncer::OS_PREFERENCES)));

  // Future changes will be synced back to browser preferences as well.
  auto* associator = static_cast<PrefModelAssociator*>(
      prefs_->GetSyncableService(syncer::PREFERENCES));
  EXPECT_TRUE(associator->IsPrefSyncedForTesting("os_pref"));
}

TEST_F(PrefServiceSyncableChromeOsTest,
       UpdatesFromOldClientsAreIgnored_Startup) {
  CreatePrefService();
  TestSyncedPrefObserver observer;
  prefs_->AddSyncedPrefObserver("os_pref", &observer);

  // Simulate an old client that has "os_pref" registered as SYNCABLE_PREF
  // instead of SYNCABLE_OS_PREF.
  syncer::SyncDataList list;
  list.push_back(CreateRemoteSyncData("os_pref", base::Value("new_value")));

  // Simulate the first sync at startup of the legacy browser prefs ModelType.
  auto* browser_associator = static_cast<PrefModelAssociator*>(
      prefs_->GetSyncableService(syncer::PREFERENCES));
  syncer::SyncChangeList outgoing_changes;
  browser_associator->MergeDataAndStartSyncing(
      syncer::PREFERENCES, list,
      std::make_unique<TestSyncProcessorStub>(&outgoing_changes));

  // No outgoing changes were triggered.
  EXPECT_TRUE(outgoing_changes.empty());

  // The value from the old client was not applied.
  EXPECT_NE("new_value", prefs_->GetString("os_pref"));

  // The pref is not considered to be syncing, because it still has its default
  // value.
  EXPECT_FALSE(browser_associator->IsPrefSyncedForTesting("os_pref"));

  // Observers were not notified of changes.
  EXPECT_EQ(0, observer.changed_count_);

  prefs_->RemoveSyncedPrefObserver("os_pref", &observer);
}

TEST_F(PrefServiceSyncableChromeOsTest,
       UpdatesFromOldClientsAreIgnored_Update) {
  CreatePrefService();
  InitSyncForAllTypes();
  TestSyncedPrefObserver observer;
  prefs_->AddSyncedPrefObserver("os_pref", &observer);

  syncer::SyncChangeList list;
  // Simulate an old client that has "os_pref" registered as SYNCABLE_PREF
  // instead of SYNCABLE_OS_PREF.
  list.push_back(MakeRemoteChange("os_pref", base::Value("new_value"),
                                  SyncChange::ACTION_ADD, syncer::PREFERENCES));

  // Simulate a sync update after startup.
  prefs_->GetSyncableService(syncer::PREFERENCES)
      ->ProcessSyncChanges(FROM_HERE, list);

  // Update was not applied.
  EXPECT_NE("new_value", prefs_->GetString("os_pref"));

  // Observers were not notified of changes.
  EXPECT_EQ(0, observer.changed_count_);

  prefs_->RemoveSyncedPrefObserver("os_pref", &observer);
}

TEST_F(PrefServiceSyncableChromeOsTest,
       SyncedPrefObserver_OsPrefIsChangedFromSync) {
  CreatePrefService();
  prefs_->SetString("os_pref", "default_value");

  TestSyncedPrefObserver observer;
  prefs_->AddSyncedPrefObserver("os_pref", &observer);

  TestPrefServiceSyncableObserver pref_service_sync_observer;
  pref_service_sync_observer.SetSyncedPrefObserver(&observer);
  prefs_->AddObserver(&pref_service_sync_observer);

  // Simulate that "os_pref" is registered as SYNCABLE_PREF
  syncer::SyncDataList list;
  list.push_back(MakeRemoteSyncData("os_pref", base::Value("new_value"),
                                    syncer::OS_PREFERENCES));

  // Simulate the first sync at startup.
  syncer::SyncChangeList outgoing_changes;
  prefs_->GetSyncableService(syncer::OS_PREFERENCES)
      ->MergeDataAndStartSyncing(
          syncer::OS_PREFERENCES, list,
          std::make_unique<TestSyncProcessorStub>(&outgoing_changes));

  EXPECT_EQ("os_pref", observer.synced_pref_);
  EXPECT_EQ(1, observer.sync_started_count_);
  EXPECT_TRUE(pref_service_sync_observer.is_syncing_changed());

  prefs_->RemoveObserver(&pref_service_sync_observer);
  prefs_->RemoveSyncedPrefObserver("os_pref", &observer);
}

TEST_F(PrefServiceSyncableChromeOsTest,
       SyncedPrefObserver_OsPrefIsNotChangedFromSync) {
  CreatePrefService();
  prefs_->SetString("os_pref", "default_value");

  TestSyncedPrefObserver observer;
  prefs_->AddSyncedPrefObserver("os_pref", &observer);

  TestPrefServiceSyncableObserver pref_service_sync_observer;
  pref_service_sync_observer.SetSyncedPrefObserver(&observer);
  prefs_->AddObserver(&pref_service_sync_observer);

  // Simulate that "os_pref" is registered as SYNCABLE_PREF
  syncer::SyncDataList list;
  list.push_back(MakeRemoteSyncData("os_pref", base::Value("new_value"),
                                    syncer::OS_PREFERENCES));

  // Simulate the first sync at startup.
  syncer::SyncChangeList outgoing_changes;
  prefs_->GetSyncableService(syncer::OS_PREFERENCES)
      ->MergeDataAndStartSyncing(
          syncer::OS_PREFERENCES, list,
          std::make_unique<TestSyncProcessorStub>(&outgoing_changes));

  EXPECT_EQ("os_pref", observer.synced_pref_);
  EXPECT_EQ(1, observer.sync_started_count_);
  EXPECT_TRUE(pref_service_sync_observer.is_syncing_changed());

  prefs_->RemoveObserver(&pref_service_sync_observer);
  prefs_->RemoveSyncedPrefObserver("os_pref", &observer);
}

TEST_F(PrefServiceSyncableChromeOsTest, SyncedPrefObserver_EmptyCloud) {
  CreatePrefService();
  prefs_->SetString("os_pref", "new_value");

  TestSyncedPrefObserver observer;
  prefs_->AddSyncedPrefObserver("os_pref", &observer);

  // Simulate the first sync at startup.
  syncer::SyncChangeList outgoing_changes;
  prefs_->GetSyncableService(syncer::OS_PREFERENCES)
      ->MergeDataAndStartSyncing(
          syncer::OS_PREFERENCES, syncer::SyncDataList(),
          std::make_unique<TestSyncProcessorStub>(&outgoing_changes));

  EXPECT_EQ("", observer.synced_pref_);
  EXPECT_EQ(0, observer.sync_started_count_);

  prefs_->RemoveSyncedPrefObserver("os_pref", &observer);
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

}  // namespace sync_preferences
