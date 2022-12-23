// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/pref_model_associator.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_store.h"
#include "components/sync_preferences/pref_model_associator_client.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_preferences {

namespace {

const char kStringPrefName[] = "pref.string";
const char kListPrefName[] = "pref.list";
const char kDictionaryPrefName[] = "pref.dictionary";
const char kCustomMergePrefName[] = "pref.custom";

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
    return pref_name == kDictionaryPrefName;
  }

  base::Value MaybeMergePreferenceValues(
      const std::string& pref_name,
      const base::Value& local_value,
      const base::Value& server_value) const override {
    if (pref_name == kCustomMergePrefName) {
      return local_value.Clone();
    }
    return base::Value();
  }
};

class AbstractPreferenceMergeTest : public testing::Test {
 protected:
  AbstractPreferenceMergeTest()
      : user_prefs_(base::MakeRefCounted<TestingPrefStore>()) {
    PrefServiceMockFactory factory;
    factory.SetPrefModelAssociatorClient(&client_);
    factory.set_user_prefs(user_prefs_);
    scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry(
        new user_prefs::PrefRegistrySyncable);
    pref_registry->RegisterStringPref(
        kStringPrefName, std::string(),
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
    pref_registry->RegisterListPref(
        kListPrefName, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
    pref_registry->RegisterDictionaryPref(
        kDictionaryPrefName, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
    pref_service_ = factory.CreateSyncable(pref_registry.get());
    pref_registry->RegisterStringPref(
        kCustomMergePrefName, std::string(),
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
    pref_sync_service_ = static_cast<PrefModelAssociator*>(
        pref_service_->GetSyncableService(syncer::PREFERENCES));
  }

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
    if (type == base::Value::Type::DICTIONARY) {
      pref_service_->SetDict(pref_name, base::Value::Dict());
    } else if (type == base::Value::Type::LIST) {
      pref_service_->SetList(pref_name, base::Value::List());
    } else {
      FAIL();
    }
  }

  TestPrefModelAssociatorClient client_;
  scoped_refptr<TestingPrefStore> user_prefs_;
  std::unique_ptr<PrefServiceSyncable> pref_service_;
  raw_ptr<PrefModelAssociator> pref_sync_service_;
};

using CustomPreferenceMergeTest = AbstractPreferenceMergeTest;

TEST_F(CustomPreferenceMergeTest, ClientMergesCustomPreference) {
  pref_service_->SetString(kCustomMergePrefName, "local");
  const PrefService::Preference* pref =
      pref_service_->FindPreference(kCustomMergePrefName);
  base::Value local_value(pref->GetValue()->Clone());
  base::Value server_value("server");
  base::Value merged_value(pref_sync_service_->MergePreference(
      pref->name(), *pref->GetValue(), server_value));
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
  base::Value server_url_list_{base::Value::Type::LIST};
};

TEST_F(ListPreferenceMergeTest, NotListOrDictionary) {
  pref_service_->SetString(kStringPrefName, local_url0_);
  const PrefService::Preference* pref =
      pref_service_->FindPreference(kStringPrefName);
  base::Value server_value(server_url0_);
  base::Value merged_value(pref_sync_service_->MergePreference(
      pref->name(), *pref->GetValue(), server_value));
  EXPECT_EQ(merged_value, server_value);
}

TEST_F(ListPreferenceMergeTest, LocalEmpty) {
  SetPrefToEmpty(kListPrefName);
  const PrefService::Preference* pref =
      pref_service_->FindPreference(kListPrefName);
  base::Value merged_value(pref_sync_service_->MergePreference(
      pref->name(), *pref->GetValue(), server_url_list_));
  EXPECT_EQ(merged_value, server_url_list_);
}

TEST_F(ListPreferenceMergeTest, ServerNull) {
  {
    ScopedListPrefUpdate update(pref_service_.get(), kListPrefName);
    update->Append(local_url0_);
  }

  const PrefService::Preference* pref =
      pref_service_->FindPreference(kListPrefName);
  base::Value merged_value(pref_sync_service_->MergePreference(
      pref->name(), *pref->GetValue(), base::Value()));
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
  base::Value merged_value(pref_sync_service_->MergePreference(
      pref->name(), *pref->GetValue(), base::Value(empty_value.Clone())));
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
  base::Value merged_value(pref_sync_service_->MergePreference(
      pref->name(), *pref->GetValue(), server_url_list_));

  base::Value::List expected;
  expected.Append(server_url0_);
  expected.Append(server_url1_);
  expected.Append(local_url0_);
  expected.Append(local_url1_);
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
  base::Value merged_value(pref_sync_service_->MergePreference(
      pref->name(), *pref->GetValue(), server_url_list_));

  base::Value::List expected;
  expected.Append(server_url0_);
  expected.Append(server_url1_);
  expected.Append(local_url0_);
  EXPECT_EQ(merged_value, expected);
}

TEST_F(ListPreferenceMergeTest, Equals) {
  {
    ScopedListPrefUpdate update(pref_service_.get(), kListPrefName);
    update->Append(server_url0_);
    update->Append(server_url1_);
  }

  base::Value original = server_url_list_.Clone();
  const PrefService::Preference* pref =
      pref_service_->FindPreference(kListPrefName);
  base::Value merged_value(pref_sync_service_->MergePreference(
      pref->name(), *pref->GetValue(), server_url_list_));
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
  base::Value merged_value(pref_sync_service_->MergePreference(
      pref->name(), *pref->GetValue(), server_patterns_));
  EXPECT_EQ(merged_value, server_patterns_);
}

TEST_F(DictionaryPreferenceMergeTest, ServerNull) {
  {
    ScopedDictPrefUpdate update(pref_service_.get(), kDictionaryPrefName);
    SetContentPattern(*update, expression3_, 1);
  }

  const PrefService::Preference* pref =
      pref_service_->FindPreference(kDictionaryPrefName);
  base::Value merged_value(pref_sync_service_->MergePreference(
      pref->name(), *pref->GetValue(), base::Value()));
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
  base::Value merged_value(pref_sync_service_->MergePreference(
      pref->name(), *pref->GetValue(), base::Value()));
  const base::Value::Dict& local_dict_value =
      pref_service_->GetDict(kDictionaryPrefName);
  EXPECT_EQ(merged_value, local_dict_value);
}

TEST_F(DictionaryPreferenceMergeTest, MergeNoConflicts) {
  {
    ScopedDictPrefUpdate update(pref_service_.get(), kDictionaryPrefName);
    SetContentPattern(*update, expression3_, 1);
  }

  base::Value merged_value(pref_sync_service_->MergePreference(
      kDictionaryPrefName,
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

  base::Value merged_value(pref_sync_service_->MergePreference(
      kDictionaryPrefName,
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

  // TODO(https://crbug.com/1187026): Migrate MergePreference() to
  // take a base::Value::Dict.
  base::Value merged_value(pref_sync_service_->MergePreference(
      kDictionaryPrefName, base::Value(local_dict_value.Clone()),
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

  base::Value merged_value(pref_sync_service_->MergePreference(
      kDictionaryPrefName,
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

  base::Value merged_value(pref_sync_service_->MergePreference(
      kDictionaryPrefName,
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

    base::Value merged_value(pref_sync_service_->MergePreference(
        pref, *pref_service_->GetUserPrefValue(pref), server_url_list_));

    base::Value::List expected;
    expected.Append(url0_);
    expected.Append(url1_);
    return merged_value == expected;
  }

  bool MergeDictionaryPreference(const char* pref) {
    {
      ScopedDictPrefUpdate update(pref_service_.get(), pref);
      SetContentPattern(*update, expression1_, 1);
    }

    base::Value merged_value(pref_sync_service_->MergePreference(
        pref, *pref_service_->GetUserPrefValue(pref), server_patterns_));

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
  base::Value server_url_list_{base::Value::Type::LIST};
  base::Value server_patterns_{base::Value::Type::DICT};
};

TEST_F(IndividualPreferenceMergeTest, ListPreference) {
  EXPECT_TRUE(MergeListPreference(kListPrefName));
}

}  // namespace

}  // namespace sync_preferences
