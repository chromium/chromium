// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/enterprise/enterprise_site_search_manager.h"

#include <string>
#include <utility>

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/template_url_data.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Pointee;
using testing::Property;

double kTimestamp = static_cast<double>(
    base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

base::Value::Dict GenerateSiteSearchPrefEntry(const std::string& keyword) {
  base::Value::Dict entry;
  entry.Set(DefaultSearchManager::kShortName, keyword + "name");
  entry.Set(DefaultSearchManager::kKeyword, keyword);
  entry.Set(DefaultSearchManager::kURL,
            std::string("http://") + keyword + ".com/{searchTerms}");
  entry.Set(DefaultSearchManager::kCreatedByPolicy,
            static_cast<int>(TemplateURLData::CreatedByPolicy::kSiteSearch));
  entry.Set(DefaultSearchManager::kEnforcedByPolicy, false);
  entry.Set(DefaultSearchManager::kFeaturedByPolicy, false);
  entry.Set(DefaultSearchManager::kFaviconURL,
            std::string("http://") + keyword + ".com/favicon.ico");
  entry.Set(DefaultSearchManager::kSafeForAutoReplace, false);
  entry.Set(DefaultSearchManager::kDateCreated, kTimestamp);
  entry.Set(DefaultSearchManager::kLastModified, kTimestamp);
  return entry;
}

class EnterpriseSiteSearchManagerTest : public testing::Test {
 public:
  EnterpriseSiteSearchManagerTest() = default;
  ~EnterpriseSiteSearchManagerTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        omnibox::kSiteSearchSettingsPolicy);

    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    EnterpriseSiteSearchManager::RegisterProfilePrefs(
        pref_service_->registry());
  }

  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return pref_service_.get();
  }

 private:
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(EnterpriseSiteSearchManagerTest, EmptyList) {
  base::MockRepeatingCallback<void(
      EnterpriseSiteSearchManager::OwnedTemplateURLDataVector&&)>
      callback;
  EXPECT_CALL(callback, Run(IsEmpty())).Times(1);

  EnterpriseSiteSearchManager manager(pref_service(), callback.Get());
  pref_service()->SetManagedPref(
      EnterpriseSiteSearchManager::kSiteSearchSettingsPrefName,
      base::Value::List());
}

TEST_F(EnterpriseSiteSearchManagerTest, NonEmptyList) {
  base::Value::List pref_value;
  pref_value.Append(GenerateSiteSearchPrefEntry("work"));
  pref_value.Append(GenerateSiteSearchPrefEntry("docs"));

  base::MockRepeatingCallback<void(
      EnterpriseSiteSearchManager::OwnedTemplateURLDataVector&&)>
      callback;
  EXPECT_CALL(
      callback,
      Run(ElementsAre(Pointee(Property(&TemplateURLData::keyword, u"work")),
                      Pointee(Property(&TemplateURLData::keyword, u"docs")))))
      .Times(1);

  EnterpriseSiteSearchManager manager(pref_service(), callback.Get());
  pref_service()->SetManagedPref(
      EnterpriseSiteSearchManager::kSiteSearchSettingsPrefName,
      std::move(pref_value));
}

TEST_F(EnterpriseSiteSearchManagerTest, NotCreatedByPolicy) {
  base::Value::List pref_value;
  pref_value.Append(GenerateSiteSearchPrefEntry("work"));
  pref_value.Append(GenerateSiteSearchPrefEntry("docs"));

  base::MockRepeatingCallback<void(
      EnterpriseSiteSearchManager::OwnedTemplateURLDataVector&&)>
      callback;
  EXPECT_CALL(callback, Run(_)).Times(0);

  EnterpriseSiteSearchManager manager(pref_service(), callback.Get());
  pref_service()->SetUserPref(
      EnterpriseSiteSearchManager::kSiteSearchSettingsPrefName,
      std::move(pref_value));
}
