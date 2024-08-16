// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/pref_model_associator.h"

#include <memory>
#include <unordered_set>

#include "base/json/json_string_value_serializer.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/prefs/mock_pref_change_callback.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_store.h"
#include "components/sync/base/features.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/preference_specifics.pb.h"
#include "components/sync/test/fake_sync_change_processor.h"
#include "components/sync_preferences/pref_model_associator_client.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/preferences_merge_helper.h"
#include "components/sync_preferences/syncable_prefs_database.h"
#include "components/sync_preferences/test_syncable_prefs_database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_preferences {

namespace {

using testing::NotNull;

const char kStringPrefName[] = "pref.string";
const char kListPrefName[] = "pref.list";
const char kDictionaryPrefName[] = "pref.dictionary";
const char kCustomMergePrefName[] = "pref.custom";

const char kStringPriorityPrefName[] = "priority.pref.string";
#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kStringOsPrefName[] = "os.pref.string";
const char kStringOsPriorityPrefName[] = "os.priority.pref.string";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Assigning an id of 0 to all the test prefs.
const TestSyncablePrefsDatabase::PrefsMap kSyncablePrefsDatabase = {
    {kStringPrefName,
     {0, syncer::PREFERENCES, PrefSensitivity::kNone, MergeBehavior::kNone}},
    {kListPrefName,
     {0, syncer::PREFERENCES, PrefSensitivity::kNone,
      MergeBehavior::kMergeableListWithRewriteOnUpdate}},
    {kDictionaryPrefName,
     {0, syncer::PREFERENCES, PrefSensitivity::kNone,
      MergeBehavior::kMergeableDict}},
    {kCustomMergePrefName,
     {0, syncer::PREFERENCES, PrefSensitivity::kNone, MergeBehavior::kCustom}},
    {kStringPriorityPrefName,
     {0, syncer::PRIORITY_PREFERENCES, PrefSensitivity::kNone,
      MergeBehavior::kNone}},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {kStringOsPrefName,
     {0, syncer::OS_PREFERENCES, PrefSensitivity::kNone, MergeBehavior::kNone}},
    {kStringOsPriorityPrefName,
     {0, syncer::OS_PRIORITY_PREFERENCES, PrefSensitivity::kNone,
      MergeBehavior::kNone}},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

// Creates SyncData for a remote pref change.
syncer::SyncData CreateRemoteSyncData(const std::string& name,
                                      base::ValueView value) {
  std::string serialized;
  JSONStringValueSerializer json(&serialized);
  EXPECT_TRUE(json.Serialize(value));
  sync_pb::EntitySpecifics specifics;
  sync_pb::PreferenceSpecifics* pref_specifics = specifics.mutable_preference();
  pref_specifics->set_name(name);
  pref_specifics->set_value(serialized);
  return syncer::SyncData::CreateRemoteData(
      specifics,
      syncer::ClientTagHash::FromUnhashed(syncer::DataType::PREFERENCES, name));
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
    if (pref_name == kCustomMergePrefName) {
      return local_value.Clone();
    }
    return base::Value();
  }

 private:
  ~TestPrefModelAssociatorClient() override = default;

  const SyncablePrefsDatabase& GetSyncablePrefsDatabase() const override {
    return syncable_prefs_database_;
  }

  TestSyncablePrefsDatabase syncable_prefs_database_;
};

scoped_refptr<user_prefs::PrefRegistrySyncable> CreatePrefRegistry() {
  scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry =
      base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();
  pref_registry->RegisterStringPref(
      kStringPrefName, std::string(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  pref_registry->RegisterListPref(
      kListPrefName, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  pref_registry->RegisterDictionaryPref(
      kDictionaryPrefName, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  pref_registry->RegisterStringPref(
      kCustomMergePrefName, std::string(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  return pref_registry;
}

std::unique_ptr<PrefServiceSyncable> CreatePrefService(
    scoped_refptr<PrefModelAssociatorClient> client,
    scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry,
    scoped_refptr<PersistentPrefStore> user_prefs) {
  PrefServiceMockFactory factory;
  factory.SetPrefModelAssociatorClient(client);
  factory.set_user_prefs(user_prefs);
  return factory.CreateSyncable(pref_registry.get());
}

class AbstractPreferenceMergeTest : public testing::Test {
 protected:
  AbstractPreferenceMergeTest() = default;

  void SetContentPattern(base::Value::Dict& patterns_dict,
                         const std::string& expression,
                         int setting) {
    base::Value::Dict* expression_dict = patterns_dict.EnsureDict(expression);
    expression_dict->Set("setting", setting);
  }

  void SetPrefToEmpty(const std::string& pref_name) {
    std::unique_ptr<base::Value> empty_value;
    const PrefService::Preference* pref =
        pref_service_->FindPreference(pref_name);
    ASSERT_TRUE(pref);
    base::Value::Type type = pref->GetType();
    if (type == base::Value::Type::DICT) {
      pref_service_->SetDict(pref_name, base::Value::Dict());
    } else if (type == base::Value::Type::LIST) {
      pref_service_->SetList(pref_name, base::Value::List());
    } else {
      FAIL();
    }
  }

  const scoped_refptr<TestPrefModelAssociatorClient> client_ =
      base::MakeRefCounted<TestPrefModelAssociatorClient>();
  const scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry_ =
      CreatePrefRegistry();
  const scoped_refptr<TestingPrefStore> user_prefs_ =
      base::MakeRefCounted<TestingPrefStore>();
  const std::unique_ptr<PrefServiceSyncable> pref_service_ =
      CreatePrefService(client_, pref_registry_, user_prefs_);
  const raw_ptr<PrefModelAssociator> pref_sync_service_ =
      static_cast<PrefModelAssociator*>(
          pref_service_->GetSyncableService(syncer::PREFERENCES));
};

using CustomPreferenceMergeTest = AbstractPreferenceMergeTest;

TEST_F(CustomPreferenceMergeTest, ClientMergesCustomPreference) {
  pref_service_->SetString(kCustomMergePrefName, "local");
  const PrefService::Preference* pref =
      pref_service_->FindPreference(kCustomMergePrefName);
  base::Value local_value(pref->GetValue()->Clone());
  base::Value server_value("server");
  base::Value merged_value(helper::MergePreference(
      client_.get(), pref->name(), *pref->GetValue(), server_value));
  // TestPrefModelAssociatorClient should have chosen local value instead of the
  // default server value.
  EXPECT_EQ(merged_value, local_value);
}

class ListPreferenceMergeTest : public AbstractPreferenceMergeTest {
 protected:
  ListPreferenceMergeTest()
      : server_url0_("http://example.com/server0"),
        server_url1_("http://example.com/server1"),
        local_url0_("http://example.com/local0"),
        local_url1_("http://example.com/local1") {
    server_url_list_.Append(server_url0_);
    server_url_list_.Append(server_url1_);
  }

  std::string server_url0_;
  std::string server_url1_;
  std::string local_url0_;
  std::string local_url1_;
  base::Value::List server_url_list_;
};

TEST_F(ListPreferenceMergeTest, NotListOrDictionary) {
  pref_service_->SetString(kStringPrefName, local_url0_);
  const PrefService::Preference* pref =
      pref_service_->FindPreference(kStringPrefName);
  base::Value server_value(server_url0_);
  base::Value merged_value(helper::MergePreference(
      client_.get(), pref->name(), *pref->GetValue(), server_value));
  EXPECT_EQ(merged_value, server_value);
}

TEST_F(ListPreferenceMergeTest, LocalEmpty) {
  SetPrefToEmpty(kListPrefName);
  const PrefService::Preference* pref =
      pref_service_->FindPreference(kListPrefName);
  base::Value merged_value(
      helper::MergePreference(client_.get(), pref->name(), *pref->GetValue(),
                              base::Value(server_url_list_.Clone())));
  EXPECT_EQ(merged_value, server_url_list_);
}

TEST_F(ListPreferenceMergeTest, ServerNull) {
  {
    ScopedListPrefUpdate update(pref_service_.get(), kListPrefName);
    update->Append(local_url0_);
  }

  const PrefService::Preference* pref =
      pref_service_->FindPreference(kListPrefName);
  base::Value merged_value(helper::MergePreference(
      client_.get(), pref->name(), *pref->GetValue(), base::Value()));
  const base::Value::List& local_list_value =
      pref_service_->GetList(kListPrefName);
  EXPECT_EQ(merged_value, local_list_value);
}

TEST_F(ListPreferenceMergeTest, ServerEmpty) {
  base::Value::List empty_value;
  {
    ScopedListPrefUpdate update(pref_service_.get(), kListPrefName);
    update->Append(local_url0_);
  }

  const PrefService::Preference* pref =
      pref_service_->FindPreference(kListPrefName);
  base::Value merged_value(
      helper::MergePreference(client_.get(), pref->name(), *pref->GetValue(),
                              base::Value(empty_value.Clone())));
  const base::Value::List& local_list_value =
      pref_service_->GetList(kListPrefName);
  EXPECT_EQ(merged_value, local_list_value);
}

TEST_F(ListPreferenceMergeTest, ServerCorrupt) {
  {
    ScopedListPrefUpdate update(pref_service_.get(), kListPrefName);
    update->Append(local_url0_);
  }

  const PrefService::Preference* pref =
      pref_service_->FindPreference(kListPrefName);
  base::Value merged_value(
      helper::MergePreference(client_.get(), pref->name(), *pref->GetValue(),
                              base::Value("corrupt-type")));
  const base::Value::List& local_list_value =
      pref_service_->GetList(kListPrefName);
  EXPECT_EQ(merged_value, local_list_value);
}

TEST_F(ListPreferenceMergeTest, Merge) {
  {
    ScopedListPrefUpdate update(pref_service_.get(), kListPrefName);
    update->Append(local_url0_);
    update->Append(local_url1_);
  }

  const PrefService::Preference* pref =
      pref_service_->FindPreference(kListPrefName);
  base::Value merged_value(
      helper::MergePreference(client_.get(), pref->name(), *pref->GetValue(),
                              base::Value(server_url_list_.Clone())));

  auto expected = base::Value::List()
                      .Append(server_url0_)
                      .Append(server_url1_)
                      .Append(local_url0_)
                      .Append(local_url1_);
  EXPECT_EQ(merged_value, expected);
}

TEST_F(ListPreferenceMergeTest, Duplicates) {
  {
    ScopedListPrefUpdate update(pref_service_.get(), kListPrefName);
    update->Append(local_url0_);
    update->Append(server_url0_);
    update->Append(server_url1_);
  }

  const PrefService::Preference* pref =
      pref_service_->FindPreference(kListPrefName);
  base::Value merged_value(
      helper::MergePreference(client_.get(), pref->name(), *pref->GetValue(),
                              base::Value(server_url_list_.Clone())));

  auto expected = base::Value::List()
                      .Append(server_url0_)
                      .Append(server_url1_)
                      .Append(local_url0_);
  EXPECT_EQ(merged_value, expected);
}

TEST_F(ListPreferenceMergeTest, Equals) {
  {
    ScopedListPrefUpdate update(pref_service_.get(), kListPrefName);
    update->Append(server_url0_);
    update->Append(server_url1_);
  }

  base::Value::List original = server_url_list_.Clone();
  const PrefService::Preference* pref =
      pref_service_->FindPreference(kListPrefName);
  base::Value merged_value(
      helper::MergePreference(client_.get(), pref->name(), *pref->GetValue(),
                              base::Value(server_url_list_.Clone())));
  EXPECT_EQ(merged_value, original);
}

class DictionaryPreferenceMergeTest : public AbstractPreferenceMergeTest {
 protected:
  DictionaryPreferenceMergeTest()
      : expression0_("expression0"),
        expression1_("expression1"),
        expression2_("expression2"),
        expression3_("expression3"),
        expression4_("expression4") {
    SetContentPattern(server_patterns_.GetDict(), expression0_, 1);
    SetContentPattern(server_patterns_.GetDict(), expression1_, 2);
    SetContentPattern(server_patterns_.GetDict(), expression2_, 1);
  }

  std::string expression0_;
  std::string expression1_;
  std::string expression2_;
  std::string expression3_;
  std::string expression4_;
  base::Value server_patterns_{base::Value::Type::DICT};
};

TEST_F(DictionaryPreferenceMergeTest, LocalEmpty) {
  SetPrefToEmpty(kDictionaryPrefName);
  const PrefService::Preference* pref =
      pref_service_->FindPreference(kDictionaryPrefName);
  base::Value merged_value(helper::MergePreference(
      client_.get(), pref->name(), *pref->GetValue(), server_patterns_));
  EXPECT_EQ(merged_value, server_patterns_);
}

TEST_F(DictionaryPreferenceMergeTest, ServerNull) {
  {
    ScopedDictPrefUpdate update(pref_service_.get(), kDictionaryPrefName);
    SetContentPattern(*update, expression3_, 1);
  }

  const PrefService::Preference* pref =
      pref_service_->FindPreference(kDictionaryPrefName);
  base::Value merged_value(helper::MergePreference(
      client_.get(), pref->name(), *pref->GetValue(), base::Value()));
  const base::Value::Dict& local_dict_value =
      pref_service_->GetDict(kDictionaryPrefName);
  EXPECT_EQ(merged_value, local_dict_value);
}

TEST_F(DictionaryPreferenceMergeTest, ServerEmpty) {
  {
    ScopedDictPrefUpdate update(pref_service_.get(), kDictionaryPrefName);
    SetContentPattern(*update, expression3_, 1);
  }

  const PrefService::Preference* pref =
      pref_service_->FindPreference(kDictionaryPrefName);
  base::Value merged_value(helper::MergePreference(
      client_.get(), pref->name(), *pref->GetValue(), base::Value()));
  const base::Value::Dict& local_dict_value =
      pref_service_->GetDict(kDictionaryPrefName);
  EXPECT_EQ(merged_value, local_dict_value);
}

TEST_F(DictionaryPreferenceMergeTest, ServerCorrupt) {
  {
    ScopedDictPrefUpdate update(pref_service_.get(), kDictionaryPrefName);
    SetContentPattern(*update, expression3_, 1);
  }

  const PrefService::Preference* pref =
      pref_service_->FindPreference(kDictionaryPrefName);
  base::Value merged_value(
      helper::MergePreference(client_.get(), pref->name(), *pref->GetValue(),
                              base::Value("corrupt-type")));
  const base::Value::Dict& local_dict_value =
      pref_service_->GetDict(kDictionaryPrefName);
  EXPECT_EQ(merged_value, local_dict_value);
}

TEST_F(DictionaryPreferenceMergeTest, MergeNoConflicts) {
  {
    ScopedDictPrefUpdate update(pref_service_.get(), kDictionaryPrefName);
    SetContentPattern(*update, expression3_, 1);
  }

  base::Value merged_value(helper::MergePreference(
      client_.get(), kDictionaryPrefName,
      *pref_service_->FindPreference(kDictionaryPrefName)->GetValue(),
      server_patterns_));

  base::Value::Dict expected;
  SetContentPattern(expected, expression0_, 1);
  SetContentPattern(expected, expression1_, 2);
  SetContentPattern(expected, expression2_, 1);
  SetContentPattern(expected, expression3_, 1);
  EXPECT_EQ(merged_value, expected);
}

TEST_F(DictionaryPreferenceMergeTest, MergeConflicts) {
  {
    ScopedDictPrefUpdate update(pref_service_.get(), kDictionaryPrefName);
    SetContentPattern(*update, expression0_, 2);
    SetContentPattern(*update, expression2_, 1);
    SetContentPattern(*update, expression3_, 1);
    SetContentPattern(*update, expression4_, 2);
  }

  base::Value merged_value(helper::MergePreference(
      client_.get(), kDictionaryPrefName,
      *pref_service_->FindPreference(kDictionaryPrefName)->GetValue(),
      server_patterns_));

  base::Value::Dict expected;
  SetContentPattern(expected, expression0_, 1);
  SetContentPattern(expected, expression1_, 2);
  SetContentPattern(expected, expression2_, 1);
  SetContentPattern(expected, expression3_, 1);
  SetContentPattern(expected, expression4_, 2);
  EXPECT_EQ(merged_value, expected);
}

TEST_F(DictionaryPreferenceMergeTest, MergeValueToDictionary) {
  base::Value::Dict local_dict_value;
  local_dict_value.Set("key", 0);

  base::Value::Dict server_dict_value;
  server_dict_value.SetByDottedPath("key.subkey", 0);

  // TODO(crbug.com/40754070): Migrate MergePreference() to
  // take a base::Value::Dict.
  base::Value merged_value(helper::MergePreference(
      client_.get(), kDictionaryPrefName, base::Value(local_dict_value.Clone()),
      base::Value(server_dict_value.Clone())));

  EXPECT_EQ(merged_value, server_dict_value);
}

TEST_F(DictionaryPreferenceMergeTest, Equal) {
  {
    ScopedDictPrefUpdate update(pref_service_.get(), kDictionaryPrefName);
    SetContentPattern(*update, expression0_, 1);
    SetContentPattern(*update, expression1_, 2);
    SetContentPattern(*update, expression2_, 1);
  }

  base::Value merged_value(helper::MergePreference(
      client_.get(), kDictionaryPrefName,
      *pref_service_->FindPreference(kDictionaryPrefName)->GetValue(),
      server_patterns_));
  EXPECT_EQ(merged_value, server_patterns_);
}

TEST_F(DictionaryPreferenceMergeTest, ConflictButServerWins) {
  {
    ScopedDictPrefUpdate update(pref_service_.get(), kDictionaryPrefName);
    SetContentPattern(*update, expression0_, 2);
    SetContentPattern(*update, expression1_, 2);
    SetContentPattern(*update, expression2_, 1);
  }

  base::Value merged_value(helper::MergePreference(
      client_.get(), kDictionaryPrefName,
      *pref_service_->FindPreference(kDictionaryPrefName)->GetValue(),
      server_patterns_));
  EXPECT_EQ(merged_value, server_patterns_);
}

class IndividualPreferenceMergeTest : public AbstractPreferenceMergeTest {
 protected:
  IndividualPreferenceMergeTest()
      : url0_("http://example.com/server0"),
        url1_("http://example.com/server1"),
        expression0_("expression0"),
        expression1_("expression1") {
    server_url_list_.Append(url0_);
    SetContentPattern(server_patterns_.GetDict(), expression0_, 1);
  }

  bool MergeListPreference(const char* pref) {
    {
      ScopedListPrefUpdate update(pref_service_.get(), pref);
      update->Append(url1_);
    }

    base::Value merged_value(helper::MergePreference(
        client_.get(), pref, *pref_service_->GetUserPrefValue(pref),
        base::Value(server_url_list_.Clone())));

    auto expected = base::Value::List().Append(url0_).Append(url1_);
    return merged_value == expected;
  }

  bool MergeDictionaryPreference(const char* pref) {
    {
      ScopedDictPrefUpdate update(pref_service_.get(), pref);
      SetContentPattern(*update, expression1_, 1);
    }

    base::Value merged_value(helper::MergePreference(
        client_.get(), pref, *pref_service_->GetUserPrefValue(pref),
        server_patterns_));

    base::Value::Dict expected;
    SetContentPattern(expected, expression0_, 1);
    SetContentPattern(expected, expression1_, 1);
    return merged_value == expected;
  }

  std::string url0_;
  std::string url1_;
  std::string expression0_;
  std::string expression1_;
  std::string content_type0_;
  base::Value::List server_url_list_;
  base::Value server_patterns_{base::Value::Type::DICT};
};

TEST_F(IndividualPreferenceMergeTest, ListPreference) {
  EXPECT_TRUE(MergeListPreference(kListPrefName));
}

class SyncablePrefsDatabaseTest : public testing::Test {
 protected:
  SyncablePrefsDatabaseTest()
      : client_(base::MakeRefCounted<TestPrefModelAssociatorClient>()),
        pref_registry_(
            base::MakeRefCounted<user_prefs::PrefRegistrySyncable>()) {
    PrefServiceMockFactory factory;
    factory.SetPrefModelAssociatorClient(client_);
    pref_service_ = factory.CreateSyncable(pref_registry_.get());
  }

  scoped_refptr<TestPrefModelAssociatorClient> client_;
  scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry_;
  std::unique_ptr<PrefServiceSyncable> pref_service_;
};

TEST_F(SyncablePrefsDatabaseTest, ShouldAllowRegisteringSyncablePrefs) {
  pref_registry_->RegisterStringPref(
      kStringPrefName, std::string(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  EXPECT_THAT(pref_service_->FindPreference(kStringPrefName), NotNull());
  pref_registry_->RegisterListPref(
      kListPrefName, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  EXPECT_THAT(pref_service_->FindPreference(kListPrefName), NotNull());
  pref_registry_->RegisterDictionaryPref(
      kDictionaryPrefName, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  EXPECT_THAT(pref_service_->FindPreference(kDictionaryPrefName), NotNull());
}

TEST_F(SyncablePrefsDatabaseTest, ShouldAllowRegisteringSyncablePriorityPrefs) {
  pref_registry_->RegisterStringPref(
      kStringPriorityPrefName, std::string(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  EXPECT_THAT(pref_service_->FindPreference(kStringPriorityPrefName),
              NotNull());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(SyncablePrefsDatabaseTest, ShouldAllowRegisteringSyncableOSPrefs) {
  pref_registry_->RegisterStringPref(
      kStringOsPrefName, std::string(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  EXPECT_THAT(pref_service_->FindPreference(kStringOsPrefName), NotNull());
}

TEST_F(SyncablePrefsDatabaseTest,
       ShouldAllowRegisteringSyncableOSPriorityPrefs) {
  pref_registry_->RegisterStringPref(
      kStringOsPriorityPrefName, std::string(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
  EXPECT_THAT(pref_service_->FindPreference(kStringOsPriorityPrefName),
              NotNull());
}
#endif
using SyncablePrefsDatabaseDeathTest = SyncablePrefsDatabaseTest;

TEST_F(SyncablePrefsDatabaseDeathTest, ShouldFailRegisteringIllegalPrefs) {
  const std::string kIllegalStringPrefName = "not-allowed_string_pref";
  const std::string kIllegalListPrefName = "not-allowed_list_pref";
  const std::string kIllegalDictPrefName = "not-allowed_dict_pref";
  const std::string kExpectedErrorMessageHint = "syncable prefs allowlist";

  EXPECT_DCHECK_DEATH_WITH(pref_registry_->RegisterStringPref(
                               kIllegalStringPrefName, std::string(),
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF),
                           kExpectedErrorMessageHint);
  EXPECT_DCHECK_DEATH_WITH(pref_registry_->RegisterListPref(
                               kIllegalListPrefName,
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF),
                           kExpectedErrorMessageHint);
  EXPECT_DCHECK_DEATH_WITH(pref_registry_->RegisterDictionaryPref(
                               kIllegalDictPrefName,
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF),
                           kExpectedErrorMessageHint);
}

TEST_F(SyncablePrefsDatabaseDeathTest,
       ShouldFailRegisteringIllegalPriorityPrefs) {
  const std::string kIllegalStringPrefName = "not-allowed_string_pref";
  const std::string kIllegalListPrefName = "not-allowed_list_pref";
  const std::string kIllegalDictPrefName = "not-allowed_dict_pref";
  const std::string kExpectedErrorMessageHint = "syncable prefs allowlist";

  EXPECT_DCHECK_DEATH_WITH(
      pref_registry_->RegisterStringPref(
          kIllegalStringPrefName, std::string(),
          user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF),
      kExpectedErrorMessageHint);
  EXPECT_DCHECK_DEATH_WITH(
      pref_registry_->RegisterListPref(
          kIllegalListPrefName,
          user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF),
      kExpectedErrorMessageHint);
  EXPECT_DCHECK_DEATH_WITH(
      pref_registry_->RegisterDictionaryPref(
          kIllegalDictPrefName,
          user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF),
      kExpectedErrorMessageHint);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(SyncablePrefsDatabaseDeathTest, ShouldFailRegisteringIllegalOSPrefs) {
  const std::string kIllegalStringPrefName = "not-allowed_string_pref";
  const std::string kIllegalListPrefName = "not-allowed_list_pref";
  const std::string kIllegalDictPrefName = "not-allowed_dict_pref";
  const std::string kExpectedErrorMessageHint = "syncable prefs allowlist";

  EXPECT_DCHECK_DEATH_WITH(
      pref_registry_->RegisterStringPref(
          kIllegalStringPrefName, std::string(),
          user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF),
      kExpectedErrorMessageHint);
  EXPECT_DCHECK_DEATH_WITH(
      pref_registry_->RegisterListPref(
          kIllegalListPrefName,
          user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF),
      kExpectedErrorMessageHint);
  EXPECT_DCHECK_DEATH_WITH(
      pref_registry_->RegisterDictionaryPref(
          kIllegalDictPrefName,
          user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF),
      kExpectedErrorMessageHint);
}

TEST_F(SyncablePrefsDatabaseDeathTest,
       ShouldFailRegisteringIllegalOSPriorityPrefs) {
  const std::string kIllegalStringPrefName = "not-allowed_string_pref";
  const std::string kIllegalListPrefName = "not-allowed_list_pref";
  const std::string kIllegalDictPrefName = "not-allowed_dict_pref";
  const std::string kExpectedErrorMessageHint = "syncable prefs allowlist";

  EXPECT_DCHECK_DEATH_WITH(
      pref_registry_->RegisterStringPref(
          kIllegalStringPrefName, std::string(),
          user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF),
      kExpectedErrorMessageHint);
  EXPECT_DCHECK_DEATH_WITH(
      pref_registry_->RegisterListPref(
          kIllegalListPrefName,
          user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF),
      kExpectedErrorMessageHint);
  EXPECT_DCHECK_DEATH_WITH(
      pref_registry_->RegisterDictionaryPref(
          kIllegalDictPrefName,
          user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF),
      kExpectedErrorMessageHint);
}
#endif

TEST_F(SyncablePrefsDatabaseDeathTest,
       ShouldFailRegisteringPrefsWithInvalidType) {
  const std::string kExpectedErrorMessageHint = "has incorrect data";
  EXPECT_DCHECK_DEATH_WITH(
      pref_registry_->RegisterStringPref(
          kStringPrefName, std::string(),
          user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF),
      kExpectedErrorMessageHint);
  EXPECT_DCHECK_DEATH_WITH(pref_registry_->RegisterStringPref(
                               kStringPriorityPrefName, std::string(),
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF),
                           kExpectedErrorMessageHint);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_DCHECK_DEATH_WITH(
      pref_registry_->RegisterStringPref(
          kStringOsPrefName, std::string(),
          user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF),
      kExpectedErrorMessageHint);
  EXPECT_DCHECK_DEATH_WITH(
      pref_registry_->RegisterStringPref(
          kStringOsPriorityPrefName, std::string(),
          user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF),
      kExpectedErrorMessageHint);
#endif
}

class PrefModelAssociatorWithPreferencesAccountStorageTest
    : public testing::Test {
 protected:
  PrefModelAssociatorWithPreferencesAccountStorageTest()
      : feature_list_(syncer::kEnablePreferencesAccountStorage),
        client_(base::MakeRefCounted<TestPrefModelAssociatorClient>()),
        pref_registry_(
            base::MakeRefCounted<user_prefs::PrefRegistrySyncable>()),
        local_pref_store_(base::MakeRefCounted<TestingPrefStore>()),
        account_pref_store_(base::MakeRefCounted<TestingPrefStore>()) {
    pref_registry_->RegisterStringPref(
        kStringPrefName, std::string(),
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

    PrefServiceMockFactory factory;
    factory.SetPrefModelAssociatorClient(client_);
    factory.set_user_prefs(local_pref_store_);
    factory.SetAccountPrefStore(account_pref_store_);
    pref_service_ = factory.CreateSyncable(pref_registry_);
    pref_model_associator_ = static_cast<PrefModelAssociator*>(
        pref_service_->GetSyncableService(syncer::PREFERENCES));
  }

  void MergeDataAndStartSyncing(const syncer::SyncDataList& initial_data) {
    auto error = pref_model_associator_->MergeDataAndStartSyncing(
        syncer::PREFERENCES, initial_data,
        std::make_unique<syncer::FakeSyncChangeProcessor>());
    EXPECT_FALSE(error.has_value());
  }

  base::test::ScopedFeatureList feature_list_;

  scoped_refptr<TestPrefModelAssociatorClient> client_;
  scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry_;
  scoped_refptr<TestingPrefStore> local_pref_store_;
  scoped_refptr<TestingPrefStore> account_pref_store_;
  std::unique_ptr<PrefServiceSyncable> pref_service_;
  raw_ptr<PrefModelAssociator> pref_model_associator_ = nullptr;
};

// Tests that no notification is issued if the effective value is unchanged upon
// initial merge.
TEST_F(PrefModelAssociatorWithPreferencesAccountStorageTest,
       ShouldNotNotifyUponInitIfSameValueExistsInLocalStoreOnly) {
  // Load value to local store before initial merge.
  local_pref_store_->SetValue(kStringPrefName, base::Value("value"), 0);
  ASSERT_EQ(pref_service_->GetString(kStringPrefName), "value");

  // Listen to pref changes.
  MockPrefChangeCallback observer(pref_service_.get());
  PrefChangeRegistrar registrar;
  registrar.Init(pref_service_.get());
  registrar.Add(kStringPrefName, observer.GetCallback());

  // No call should be made to the observer as the effective value of the pref
  // is unchanged.
  EXPECT_CALL(observer, OnPreferenceChanged).Times(0);

  // Create initial sync data with the same pref value as that in the local
  // store.
  syncer::SyncDataList initial_data;
  initial_data.push_back(
      CreateRemoteSyncData(kStringPrefName, base::Value("value")));

  MergeDataAndStartSyncing(initial_data);
}

// Tests that no notification is issued if the effective value is unchanged upon
// initial merge.
TEST_F(PrefModelAssociatorWithPreferencesAccountStorageTest,
       ShouldNotNotifyUponInitIfSameValueExistsInAccountStoreOnly) {
  // Load value to account store before initial merge.
  account_pref_store_->SetValue(kStringPrefName, base::Value("value"), 0);
  ASSERT_EQ(pref_service_->GetString(kStringPrefName), "value");

  // Listen to pref changes.
  MockPrefChangeCallback observer(pref_service_.get());
  PrefChangeRegistrar registrar;
  registrar.Init(pref_service_.get());
  registrar.Add(kStringPrefName, observer.GetCallback());

  // No call should be made to the observer as the effective value of the pref
  // is unchanged.
  EXPECT_CALL(observer, OnPreferenceChanged).Times(0);

  // Create initial sync data with the same pref value as that in the local
  // store.
  syncer::SyncDataList initial_data;
  initial_data.push_back(
      CreateRemoteSyncData(kStringPrefName, base::Value("value")));

  MergeDataAndStartSyncing(initial_data);
}

// Tests that notification is issued if the effective value changes upon
// initial merge.
TEST_F(PrefModelAssociatorWithPreferencesAccountStorageTest,
       ShouldNotifyUponInitIfDifferentValueExistsInLocalStoreOnly) {
  // Load value to local store before initial merge.
  local_pref_store_->SetValue(kStringPrefName, base::Value("value"), 0);
  ASSERT_EQ(pref_service_->GetString(kStringPrefName), "value");

  // Listen to pref changes.
  MockPrefChangeCallback observer(pref_service_.get());
  PrefChangeRegistrar registrar;
  registrar.Init(pref_service_.get());
  registrar.Add(kStringPrefName, observer.GetCallback());

  // Observer should get notified since the effective value changes.
  EXPECT_CALL(observer, OnPreferenceChanged);

  // Create initial sync data with a different pref value than that in the
  // local store.
  syncer::SyncDataList initial_data;
  initial_data.push_back(
      CreateRemoteSyncData(kStringPrefName, base::Value("new value")));

  MergeDataAndStartSyncing(initial_data);
  ASSERT_EQ(pref_service_->GetString(kStringPrefName), "new value");
}

// Tests that notification is issued if the effective value changes upon
// initial merge.
TEST_F(PrefModelAssociatorWithPreferencesAccountStorageTest,
       ShouldNotifyUponInitIfDifferentValueExistsInAccountStoreOnly) {
  // Load value to account store before initial merge.
  account_pref_store_->SetValue(kStringPrefName, base::Value("value"), 0);
  ASSERT_EQ(pref_service_->GetString(kStringPrefName), "value");

  // Listen to pref changes.
  MockPrefChangeCallback observer(pref_service_.get());
  PrefChangeRegistrar registrar;
  registrar.Init(pref_service_.get());
  registrar.Add(kStringPrefName, observer.GetCallback());

  // Observer should get notified since the effective value changes.
  EXPECT_CALL(observer, OnPreferenceChanged);

  // Create initial sync data with a different pref value than that in the
  // local store.
  syncer::SyncDataList initial_data;
  initial_data.push_back(
      CreateRemoteSyncData(kStringPrefName, base::Value("new value")));

  MergeDataAndStartSyncing(initial_data);
  ASSERT_EQ(pref_service_->GetString(kStringPrefName), "new value");
}

}  // namespace

}  // namespace sync_preferences
