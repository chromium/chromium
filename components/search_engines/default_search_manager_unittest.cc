// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/default_search_manager.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_test_util.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// TODO(caitkp): TemplateURLData-ify this.
void SetOverrides(sync_preferences::TestingPrefServiceSyncable* prefs,
                  bool update) {
  prefs->SetUserPref(prefs::kSearchProviderOverridesVersion, base::Value(1));

  auto entry1 =
      base::Value::Dict()
          .Set("name", update ? "new_foo" : "foo")
          .Set("keyword", update ? "new_fook" : "fook")
          .Set("search_url", "http://foo.com/s?q={searchTerms}")
          .Set("favicon_url", "http://foi.com/favicon.ico")
          .Set("encoding", "UTF-8")
          .Set("id", 1001)
          .Set("suggest_url", "http://foo.com/suggest?q={searchTerms}")
          .Set("alternate_urls",
               base::Value::List().Append(
                   "http://foo.com/alternate?q={searchTerms}"));

  auto entry2 = base::Value::Dict()
                    .Set("id", 1002)
                    .Set("name", update ? "new_bar" : "bar")
                    .Set("keyword", update ? "new_bark" : "bark")
                    .Set("encoding", std::string());

  auto entry3 = base::Value::Dict()
                    .Set("id", 1003)
                    .Set("name", "baz")
                    .Set("keyword", "bazk")
                    .Set("encoding", "UTF-8");

  auto overrides = base::Value::List()
                       .Append(std::move(entry1))
                       .Append(std::move(entry2))
                       .Append(std::move(entry3));

  prefs->SetUserPref(prefs::kSearchProviderOverrides, std::move(overrides));
}

void SetPolicy(sync_preferences::TestingPrefServiceSyncable* prefs,
               bool enabled,
               TemplateURLData* data,
               bool is_mandatory) {
  if (enabled) {
    EXPECT_FALSE(data->keyword().empty());
    EXPECT_FALSE(data->url().empty());
  }
  base::Value::Dict entry = TemplateURLDataToDictionary(*data);
  entry.Set(DefaultSearchManager::kDisabledByPolicy, !enabled);

  is_mandatory ? prefs->SetManagedPref(
                     DefaultSearchManager::kDefaultSearchProviderDataPrefName,
                     std::move(entry))
               : prefs->SetRecommendedPref(
                     DefaultSearchManager::kDefaultSearchProviderDataPrefName,
                     std::move(entry));
}

}  // namespace

class DefaultSearchManagerTest : public testing::Test {
 public:
  DefaultSearchManagerTest() {}

  DefaultSearchManagerTest(const DefaultSearchManagerTest&) = delete;
  DefaultSearchManagerTest& operator=(const DefaultSearchManagerTest&) = delete;

  void SetUp() override {
    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    DefaultSearchManager::RegisterProfilePrefs(pref_service_->registry());
    TemplateURLPrepopulateData::RegisterProfilePrefs(pref_service_->registry());
  }

  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return pref_service_.get();
  }

 private:
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
};

// Test that a TemplateURLData object is properly written and read from Prefs.
TEST_F(DefaultSearchManagerTest, ReadAndWritePref) {
  DefaultSearchManager manager(pref_service(),
                               DefaultSearchManager::ObserverCallback());
  TemplateURLData data;
  data.SetShortName(u"name1");
  data.SetKeyword(u"key1");
  data.SetURL("http://foo1/{searchTerms}");
  data.suggestions_url = "http://sugg1";
  data.alternate_urls.push_back("http://foo1/alt");
  data.favicon_url = GURL("http://icon1");
  data.safe_for_autoreplace = true;
  data.input_encodings = base::SplitString(
      "UTF-8;UTF-16", ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  data.date_created = base::Time();
  data.last_modified = base::Time();
  data.last_modified = base::Time();
  data.created_from_play_api = true;

  manager.SetUserSelectedDefaultSearchEngine(data);
  const TemplateURLData* read_data = manager.GetDefaultSearchEngine(nullptr);
  ExpectSimilar(&data, read_data);
}

// Test DefaultSearchmanager handles user-selected DSEs correctly.
TEST_F(DefaultSearchManagerTest, DefaultSearchSetByUserPref) {
  size_t default_search_index = 0;
  DefaultSearchManager manager(pref_service(),
                               DefaultSearchManager::ObserverCallback());
  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_urls =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(pref_service(),
                                                         &default_search_index);
  DefaultSearchManager::Source source = DefaultSearchManager::FROM_POLICY;
  // If no user pref is set, we should use the pre-populated values.
  ExpectSimilar(prepopulated_urls[default_search_index].get(),
                manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_FALLBACK, source);

  // Setting a user pref overrides the pre-populated values.
  std::unique_ptr<TemplateURLData> data = GenerateDummyTemplateURLData("user");
  manager.SetUserSelectedDefaultSearchEngine(*data);

  ExpectSimilar(data.get(), manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);

  // Updating the user pref (externally to this instance of
  // DefaultSearchManager) triggers an update.
  std::unique_ptr<TemplateURLData> new_data =
      GenerateDummyTemplateURLData("user2");
  DefaultSearchManager other_manager(pref_service(),
                                     DefaultSearchManager::ObserverCallback());
  other_manager.SetUserSelectedDefaultSearchEngine(*new_data);

  ExpectSimilar(new_data.get(), manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);

  // Clearing the user pref should cause the default search to revert to the
  // prepopulated vlaues.
  manager.ClearUserSelectedDefaultSearchEngine();
  ExpectSimilar(prepopulated_urls[default_search_index].get(),
                manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_FALLBACK, source);
}

// Test that DefaultSearch manager detects changes to kSearchProviderOverrides.
TEST_F(DefaultSearchManagerTest, DefaultSearchSetByOverrides) {
  SetOverrides(pref_service(), false);
  size_t default_search_index = 0;
  DefaultSearchManager manager(pref_service(),
                               DefaultSearchManager::ObserverCallback());
  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_urls =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(pref_service(),
                                                         &default_search_index);

  DefaultSearchManager::Source source = DefaultSearchManager::FROM_POLICY;
  TemplateURLData first_default(*manager.GetDefaultSearchEngine(&source));
  ExpectSimilar(prepopulated_urls[default_search_index].get(), &first_default);
  EXPECT_EQ(DefaultSearchManager::FROM_FALLBACK, source);

  // Update the overrides:
  SetOverrides(pref_service(), true);
  prepopulated_urls = TemplateURLPrepopulateData::GetPrepopulatedEngines(
      pref_service(), &default_search_index);

  // Make sure DefaultSearchManager updated:
  ExpectSimilar(prepopulated_urls[default_search_index].get(),
                manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_FALLBACK, source);
  EXPECT_NE(manager.GetDefaultSearchEngine(nullptr)->short_name(),
            first_default.short_name());
  EXPECT_NE(manager.GetDefaultSearchEngine(nullptr)->keyword(),
            first_default.keyword());
}

// Test DefaultSearchManager handles policy-enforced DSEs correctly.
TEST_F(DefaultSearchManagerTest, DefaultSearchSetByEnforcedPolicy) {
  DefaultSearchManager manager(pref_service(),
                               DefaultSearchManager::ObserverCallback());
  std::unique_ptr<TemplateURLData> data = GenerateDummyTemplateURLData("user");
  manager.SetUserSelectedDefaultSearchEngine(*data);

  DefaultSearchManager::Source source = DefaultSearchManager::FROM_FALLBACK;
  ExpectSimilar(data.get(), manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);

  std::unique_ptr<TemplateURLData> policy_data =
      GenerateDummyTemplateURLData("policy");
  SetPolicy(pref_service(), true, policy_data.get(), /*is_mandatory=*/true);

  ExpectSimilar(policy_data.get(), manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_POLICY, source);

  TemplateURLData null_policy_data;
  SetPolicy(pref_service(), false, &null_policy_data, /*is_mandatory=*/true);
  EXPECT_EQ(nullptr, manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_POLICY, source);

  pref_service()->RemoveManagedPref(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName);
  ExpectSimilar(data.get(), manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);
}

// Policy-recommended DSE is handled correctly when no existing DSE is present.
TEST_F(DefaultSearchManagerTest, DefaultSearchSetByRecommendedPolicy) {
  DefaultSearchManager manager(pref_service(),
                               DefaultSearchManager::ObserverCallback());
  DefaultSearchManager::Source source = DefaultSearchManager::FROM_FALLBACK;

  // Set recommended policy DSE with valid data.
  std::unique_ptr<TemplateURLData> policy_data =
      GenerateDummyTemplateURLData("policy");
  SetPolicy(pref_service(), true, policy_data.get(), /*is_mandatory=*/false);
  ExpectSimilar(policy_data.get(), manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_POLICY_RECOMMENDED, source);

  // Set recommended policy DSE with null data.
  TemplateURLData null_policy_data;
  SetPolicy(pref_service(), false, &null_policy_data, /*is_mandatory=*/false);
  EXPECT_EQ(nullptr, manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_POLICY_RECOMMENDED, source);

  // Set user-configured DSE.
  std::unique_ptr<TemplateURLData> user_data =
      GenerateDummyTemplateURLData("user");
  manager.SetUserSelectedDefaultSearchEngine(*user_data);
  // The user-configured DSE overrides the recommended policy DSE.
  ExpectSimilar(user_data.get(), manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);

  // Remove the recommended policy DSE.
  pref_service()->RemoveRecommendedPref(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName);
  ExpectSimilar(user_data.get(), manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);
}

// Policy-recommended DSE does not override existing DSE set by user.
TEST_F(DefaultSearchManagerTest, DefaultSearchSetByUserAndRecommendedPolicy) {
  DefaultSearchManager manager(pref_service(),
                               DefaultSearchManager::ObserverCallback());
  // Set user-configured DSE.
  std::unique_ptr<TemplateURLData> user_data =
      GenerateDummyTemplateURLData("user");
  manager.SetUserSelectedDefaultSearchEngine(*user_data);
  DefaultSearchManager::Source source = DefaultSearchManager::FROM_FALLBACK;
  ExpectSimilar(user_data.get(), manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);

  // Set recommended policy DSE.
  std::unique_ptr<TemplateURLData> policy_data =
      GenerateDummyTemplateURLData("policy");
  SetPolicy(pref_service(), true, policy_data.get(), /*is_mandatory=*/false);
  // The recommended policy DSE does not override the existing user DSE.
  ExpectSimilar(user_data.get(), manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);

  // Remove the recommended policy DSE.
  pref_service()->RemoveRecommendedPref(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName);
  ExpectSimilar(user_data.get(), manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);
}

// Test DefaultSearchManager handles extension-controlled DSEs correctly.
TEST_F(DefaultSearchManagerTest, DefaultSearchSetByExtension) {
  DefaultSearchManager manager(pref_service(),
                               DefaultSearchManager::ObserverCallback());
  std::unique_ptr<TemplateURLData> data = GenerateDummyTemplateURLData("user");
  manager.SetUserSelectedDefaultSearchEngine(*data);

  DefaultSearchManager::Source source = DefaultSearchManager::FROM_FALLBACK;
  ExpectSimilar(data.get(), manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);

  // Extension trumps prefs:
  std::unique_ptr<TemplateURLData> extension_data_1 =
      GenerateDummyTemplateURLData("ext1");
  SetExtensionDefaultSearchInPrefs(pref_service(), *extension_data_1);
  ExpectSimilar(extension_data_1.get(),
                manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_EXTENSION, source);

  // Policy trumps extension:
  std::unique_ptr<TemplateURLData> policy_data =
      GenerateDummyTemplateURLData("policy");
  SetPolicy(pref_service(), true, policy_data.get(), /*is_mandatory=*/true);

  ExpectSimilar(policy_data.get(), manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_POLICY, source);
  pref_service()->RemoveManagedPref(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName);

  // Extensions trump each other:
  std::unique_ptr<TemplateURLData> extension_data_2 =
      GenerateDummyTemplateURLData("ext2");
  std::unique_ptr<TemplateURLData> extension_data_3 =
      GenerateDummyTemplateURLData("ext3");

  SetExtensionDefaultSearchInPrefs(pref_service(), *extension_data_2);
  SetExtensionDefaultSearchInPrefs(pref_service(), *extension_data_3);
  ExpectSimilar(extension_data_3.get(),
                manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_EXTENSION, source);

  RemoveExtensionDefaultSearchFromPrefs(pref_service());
  ExpectSimilar(data.get(), manager.GetDefaultSearchEngine(&source));
  EXPECT_EQ(DefaultSearchManager::FROM_USER, source);
}

// Verify that DefaultSearchManager preserves search engine parameters for
// search engine created from Play API data.
TEST_F(DefaultSearchManagerTest, DefaultSearchSetByPlayAPI) {
  DefaultSearchManager manager(pref_service(),
                               DefaultSearchManager::ObserverCallback());
  const TemplateURLData* prepopulated_data =
      manager.GetDefaultSearchEngine(nullptr);

  // The test tries to set DSE to the one with prepopulate_id, matching existing
  // prepopulated search engine.
  std::unique_ptr<TemplateURLData> data = GenerateDummyTemplateURLData(
      base::UTF16ToUTF8(prepopulated_data->keyword()));
  data->prepopulate_id = prepopulated_data->prepopulate_id;
  data->favicon_url = prepopulated_data->favicon_url;

  // If the new search engine was not created form Play API data its parameters
  // should be overwritten with prepopulated data.
  manager.SetUserSelectedDefaultSearchEngine(*data);
  const TemplateURLData* read_data = manager.GetDefaultSearchEngine(nullptr);
  ExpectSimilar(prepopulated_data, read_data);

  // If the new search engine was created form Play API data its parameters
  // should be preserved.
  data->created_from_play_api = true;
  manager.SetUserSelectedDefaultSearchEngine(*data);
  read_data = manager.GetDefaultSearchEngine(nullptr);
  ExpectSimilar(data.get(), read_data);
}
