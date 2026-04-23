// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_engines/keyword_editor_controller.h"

#include <array>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/browser/ui/search_engines/template_url_table_model.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/choice_made_location.h"
#include "components/search_engines/enterprise/enterprise_search_manager.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

static const std::u16string kA(u"a");
static const std::u16string kA1(u"a1");
static const std::u16string kB(u"b");
static const std::u16string kB1(u"b1");
static const std::u16string kManaged(u"managed");

// Base class for keyword editor tests. Creates a profile containing an
// empty TemplateURLService.
class KeywordEditorControllerTest : public testing::Test,
                                    public TemplateURLServiceObserver {
 public:
  KeywordEditorControllerTest()
      : util_(&profile_),
        simulate_load_failure_(false),
        model_changed_count_(0) {}

  explicit KeywordEditorControllerTest(bool simulate_load_failure)
      : util_(&profile_),
        simulate_load_failure_(simulate_load_failure),
        model_changed_count_(0) {}

  void SetUp() override {
    if (simulate_load_failure_) {
      util_.model()->OnWebDataServiceRequestDone(0, nullptr);
    } else {
      util_.VerifyLoad();
    }

    controller_ = std::make_unique<KeywordEditorController>(&profile_);
    scoped_url_service_observation_.Observe(util_.model());
  }

  void TearDown() override { controller_.reset(); }

  void VerifyChanged() {
    ASSERT_EQ(1, model_changed_count_);
    ClearChangeCount();
  }

  void VerifyNotChanged() { ASSERT_EQ(0, model_changed_count_); }

  void ClearChangeCount() { model_changed_count_ = 0; }

  void SimulateDefaultSearchIsManaged(const std::string& url,
                                      bool is_mandatory) {
    TemplateURLData managed_engine;
    managed_engine.SetShortName(kManaged);
    managed_engine.SetKeyword(kManaged);
    managed_engine.SetURL(url);
    managed_engine.policy_origin =
        TemplateURLData::PolicyOrigin::kDefaultSearchProvider;
    managed_engine.enforced_by_policy = is_mandatory;
    is_mandatory
        ? SetManagedDefaultSearchPreferences(managed_engine, true, &profile_)
        : SetRecommendedDefaultSearchPreferences(managed_engine, true,
                                                 &profile_);
  }

  // TemplateURLServiceObserver implementation. The controller would usually be
  // notified by the search engines handler. For the sake of testing, simulate
  // this linking.
  void OnTemplateURLServiceChanged() override {
    model_changed_count_++;
    controller()->Refresh();
  }

  TemplateURLTableModel* table_model() { return controller_->table_model(); }
  KeywordEditorController* controller() { return controller_.get(); }
  const TemplateURLServiceFactoryTestUtil* util() const { return &util_; }
  const TestingProfile& profile() const { return profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<KeywordEditorController> controller_;
  TemplateURLServiceFactoryTestUtil util_;
  bool simulate_load_failure_;
  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      scoped_url_service_observation_{this};

  int model_changed_count_;
};

class KeywordEditorControllerNoWebDataTest
    : public KeywordEditorControllerTest {
 public:
  KeywordEditorControllerNoWebDataTest() : KeywordEditorControllerTest(true) {}
};

class KeywordEditorControllerManagedDSPTest
    : public KeywordEditorControllerTest {
 public:
  KeywordEditorControllerManagedDSPTest()
      : KeywordEditorControllerTest(false) {}
  ~KeywordEditorControllerManagedDSPTest() override = default;
};

// Tests adding a TemplateURL.
TEST_F(KeywordEditorControllerTest, Add) {
  size_t original_row_count = table_model()->engine_count();
  controller()->AddTemplateURL(kA, kB, "http://c");

  // Verify the observer was notified.
  VerifyChanged();
  if (HasFatalFailure()) {
    return;
  }

  // Verify the TableModel has the new data.
  ASSERT_EQ(original_row_count + 1, table_model()->engine_count());

  // Verify the TemplateURLService has the new data.
  const TemplateURL* turl = util()->model()->GetTemplateURLForKeyword(kB);
  ASSERT_TRUE(turl);
  EXPECT_EQ(u"a", turl->short_name());
  EXPECT_EQ(u"b", turl->keyword());
  EXPECT_EQ("http://c", turl->url());
}

// Tests modifying a TemplateURL.
TEST_F(KeywordEditorControllerTest, Modify) {
  controller()->AddTemplateURL(kA, kB, "http://c");
  ClearChangeCount();

  // Modify the entry.
  TemplateURL* turl = util()->model()->GetTemplateURLs()[0];
  controller()->ModifyTemplateURL(turl, kA1, kB1, "http://c1");

  // Make sure it was updated appropriately.
  VerifyChanged();
  EXPECT_EQ(u"a1", turl->short_name());
  EXPECT_EQ(u"b1", turl->keyword());
  EXPECT_EQ("http://c1", turl->url());

  // Verify preference was not updated.
  const base::ListValue& overridden_keywords = profile().GetPrefs()->GetList(
      EnterpriseSearchManager::kSiteSearchSettingsOverriddenKeywordsPrefName);
  EXPECT_TRUE(overridden_keywords.empty());
}

// Regression test for crbug.com/499223471.
TEST_F(KeywordEditorControllerTest, ModifyWithoutChange) {
  const TemplateURLID turl_id =
      controller()->AddTemplateURL(kA, kB, "c1.com/?s=%s");
  ClearChangeCount();

  // Trigger a modification, but the URL is the same as the original one, only
  // in a fixed up format.
  TemplateURL* turl = controller()->GetTemplateURL(turl_id);
  controller()->ModifyTemplateURL(turl, kA, kB,
                                  "http://c1.com/?s={searchTerms}");

  // Make sure it was not updated.
  VerifyNotChanged();
  EXPECT_EQ(u"a", turl->short_name());
  EXPECT_EQ(u"b", turl->keyword());
  EXPECT_EQ("c1.com/?s=%s", turl->url());

  // Verify preference was not updated.
  const base::ListValue& overridden_keywords = profile().GetPrefs()->GetList(
      EnterpriseSearchManager::kSiteSearchSettingsOverriddenKeywordsPrefName);
  EXPECT_TRUE(overridden_keywords.empty());
}

// Tests modifying a SiteSearch TemplateURL.
TEST_F(KeywordEditorControllerTest, Modify_SiteSearchPolicyEngine) {
  // Create an entry from Site Search policy.
  TemplateURLData data;
  data.SetShortName(kA);
  data.SetKeyword(kB);
  data.SetURL("http://c");
  data.policy_origin = TemplateURLData::PolicyOrigin::kSiteSearch;
  TemplateURL* turl = util()->model()->Add(std::make_unique<TemplateURL>(data));
  ClearChangeCount();

  // Modify the entry.
  controller()->ModifyTemplateURL(turl, kA1, kB1, "http://c1");

  // Make sure it was updated appropriately.
  VerifyChanged();
  EXPECT_EQ(u"a1", turl->short_name());
  EXPECT_EQ(u"b1", turl->keyword());
  EXPECT_EQ("http://c1", turl->url());

  // Verify preference was updated to include keyword.
  const base::ListValue& overridden_keywords = profile().GetPrefs()->GetList(
      EnterpriseSearchManager::kSiteSearchSettingsOverriddenKeywordsPrefName);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(1u, overridden_keywords.size());
  EXPECT_EQ(base::UTF16ToUTF8(kB), overridden_keywords[0].GetString());
#else
  EXPECT_TRUE(overridden_keywords.empty());
#endif
}

// Tests removing a TemplateURL.
TEST_F(KeywordEditorControllerTest, Remove) {
  TemplateURLID id = controller()->AddTemplateURL(kA, kB, "http://c");
  auto original_size = util()->model()->GetTemplateURLs().size();
  ClearChangeCount();

  // Remove the entry.
  controller()->RemoveTemplateURL(id);

  // Make sure it was deleted appropriately.
  VerifyChanged();
  EXPECT_FALSE(util()->model()->GetTemplateURLForKeyword(kB));
  EXPECT_EQ(original_size - 1, util()->model()->GetTemplateURLs().size());

  // Verify preference was not updated.
  const base::ListValue& overridden_keywords = profile().GetPrefs()->GetList(
      EnterpriseSearchManager::kSiteSearchSettingsOverriddenKeywordsPrefName);
  EXPECT_TRUE(overridden_keywords.empty());
}

// Tests removing a SiteSearch TemplateURL.
TEST_F(KeywordEditorControllerTest, Remove_SiteSearchPolicyEngine) {
  // Create an entry from Site Search policy.
  TemplateURLData data;
  data.SetShortName(kA);
  data.SetKeyword(kB);
  data.SetURL("http://c");
  data.policy_origin = TemplateURLData::PolicyOrigin::kSiteSearch;
  TemplateURL* turl = util()->model()->Add(std::make_unique<TemplateURL>(data));
  auto original_size = util()->model()->GetTemplateURLs().size();
  ClearChangeCount();

  // Remove the entry.
  controller()->RemoveTemplateURL(turl->id());

  // Make sure it was deleted appropriately.
  VerifyChanged();
  EXPECT_FALSE(util()->model()->GetTemplateURLForKeyword(kB));
  EXPECT_EQ(original_size - 1, util()->model()->GetTemplateURLs().size());

  // Verify preference was updated to include keyword.
  const base::ListValue& overridden_keywords = profile().GetPrefs()->GetList(
      EnterpriseSearchManager::kSiteSearchSettingsOverriddenKeywordsPrefName);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(1u, overridden_keywords.size());
  EXPECT_EQ(base::UTF16ToUTF8(kB), overridden_keywords[0].GetString());
#else
  EXPECT_TRUE(overridden_keywords.empty());
#endif
}

// Tests making a TemplateURL the default search provider.
TEST_F(KeywordEditorControllerTest, MakeDefault) {
  TemplateURLID id =
      controller()->AddTemplateURL(kA, kB, "http://c{searchTerms}");
  ClearChangeCount();

  const TemplateURL* turl = util()->model()->GetTemplateURLForKeyword(kB);
  controller()->MakeDefaultTemplateURL(
      id, search_engines::ChoiceMadeLocation::kOther);
  // Making an item the default sends a handful of changes. Which are sent isn't
  // important, what is important is 'something' is sent.
  VerifyChanged();
  ASSERT_EQ(turl, util()->model()->GetDefaultSearchProvider());

  // Making it default a second time should fail.
  controller()->MakeDefaultTemplateURL(
      id, search_engines::ChoiceMadeLocation::kOther);
  EXPECT_EQ(turl, util()->model()->GetDefaultSearchProvider());
}

// Tests that a TemplateURL can't be made the default if the default search
// provider is managed via policy.
TEST_F(KeywordEditorControllerManagedDSPTest, CannotSetDefaultWhileManaged) {
  controller()->AddTemplateURL(kA, kB, "http://c{searchTerms}");
  controller()->AddTemplateURL(kA1, kB1, "http://d{searchTerms}");
  ClearChangeCount();

  const TemplateURL* turl1 = util()->model()->GetTemplateURLForKeyword(u"b");
  ASSERT_NE(turl1, nullptr);
  const TemplateURL* turl2 = util()->model()->GetTemplateURLForKeyword(u"b1");
  ASSERT_NE(turl2, nullptr);

  EXPECT_TRUE(controller()->CanMakeDefault(turl1));
  EXPECT_FALSE(controller()->IsManaged(turl1));
  EXPECT_TRUE(controller()->CanMakeDefault(turl2));
  EXPECT_FALSE(controller()->IsManaged(turl2));

  SimulateDefaultSearchIsManaged(turl2->url(), /*is_mandatory=*/true);
  EXPECT_TRUE(util()->model()->is_default_search_managed());

  EXPECT_FALSE(controller()->CanMakeDefault(turl1));
  EXPECT_FALSE(controller()->IsManaged(turl1));
  EXPECT_FALSE(controller()->CanMakeDefault(turl2));
  EXPECT_TRUE(
      controller()->IsManaged(util()->model()->GetDefaultSearchProvider()));
}

// Tests that a TemplateURL can be made the default if the default search
// provider is recommended via policy.
TEST_F(KeywordEditorControllerManagedDSPTest, SetDefaultWhileRecommended) {
  controller()->AddTemplateURL(kA, kB, "http://c{searchTerms}");
  ClearChangeCount();
  const TemplateURL* turl1 = util()->model()->GetTemplateURLForKeyword(kB);
  ASSERT_NE(turl1, nullptr);
  EXPECT_TRUE(controller()->CanMakeDefault(turl1));

  // Simulate setting a recommended default provider. This adds another template
  // URL to the model.
  SimulateDefaultSearchIsManaged(turl1->url(), /*is_mandatory=*/false);
  EXPECT_EQ(kManaged,
            util()->model()->GetDefaultSearchProvider()->short_name());
  EXPECT_FALSE(util()->model()->is_default_search_managed());
  EXPECT_TRUE(controller()->CanMakeDefault(turl1));
  EXPECT_FALSE(
      controller()->IsManaged(util()->model()->GetDefaultSearchProvider()));

  TemplateURLID id =
      controller()->AddTemplateURL(kA1, kB1, "http://d{searchTerms}");
  ClearChangeCount();
  const TemplateURL* turl2 = util()->model()->GetTemplateURLForKeyword(kB1);
  ASSERT_NE(turl2, nullptr);
  EXPECT_TRUE(controller()->CanMakeDefault(turl2));

  // Update the default search provider.
  EXPECT_NE(turl2, util()->model()->GetDefaultSearchProvider());
  controller()->MakeDefaultTemplateURL(
      id, search_engines::ChoiceMadeLocation::kOther);
  VerifyChanged();
  EXPECT_EQ(turl2, util()->model()->GetDefaultSearchProvider());

  // Ensure that the recommended search provider is not deleted.
  ASSERT_NE(util()->model()->GetTemplateURLForKeyword(kManaged), nullptr);
}

// Tests that a recomended search provider does not persist when a different
// recommended provider is applied via policy.
TEST_F(KeywordEditorControllerManagedDSPTest, UpdateRecommended) {
  // Simulate setting a recommended default provider.
  SimulateDefaultSearchIsManaged("url1", /*is_mandatory=*/false);
  EXPECT_EQ(kManaged,
            util()->model()->GetDefaultSearchProvider()->short_name());
  EXPECT_EQ("url1", util()->model()->GetDefaultSearchProvider()->url());
  EXPECT_FALSE(util()->model()->is_default_search_managed());
  EXPECT_FALSE(
      controller()->IsManaged(util()->model()->GetDefaultSearchProvider()));
  auto original_size = util()->model()->GetTemplateURLs().size();

  // Update the default search provider to a different recommended provider.
  SimulateDefaultSearchIsManaged("url2", /*is_mandatory=*/false);
  EXPECT_EQ("url2", util()->model()->GetDefaultSearchProvider()->url());
  EXPECT_FALSE(util()->model()->is_default_search_managed());
  EXPECT_FALSE(
      util()->model()->GetDefaultSearchProvider()->enforced_by_policy());
  EXPECT_FALSE(
      controller()->IsManaged(util()->model()->GetDefaultSearchProvider()));
  EXPECT_EQ(original_size, util()->model()->GetTemplateURLs().size());
}

// Tests that a recomended search provider does not persist when a managed
// provider is applied via policy.
TEST_F(KeywordEditorControllerManagedDSPTest, SetManagedWhileRecommended) {
  // Simulate setting a recommended default provider.
  SimulateDefaultSearchIsManaged("url1", /*is_mandatory=*/false);
  EXPECT_EQ(kManaged,
            util()->model()->GetDefaultSearchProvider()->short_name());
  EXPECT_EQ("url1", util()->model()->GetDefaultSearchProvider()->url());
  EXPECT_FALSE(util()->model()->is_default_search_managed());
  EXPECT_FALSE(
      controller()->IsManaged(util()->model()->GetDefaultSearchProvider()));
  auto original_size = util()->model()->GetTemplateURLs().size();

  // Update the default search provider to a managed (enforced) provider.
  SimulateDefaultSearchIsManaged("url2", /*is_mandatory=*/true);
  EXPECT_EQ("url2", util()->model()->GetDefaultSearchProvider()->url());
  EXPECT_TRUE(util()->model()->is_default_search_managed());
  EXPECT_TRUE(
      util()->model()->GetDefaultSearchProvider()->enforced_by_policy());
  EXPECT_TRUE(
      controller()->IsManaged(util()->model()->GetDefaultSearchProvider()));
  EXPECT_EQ(original_size, util()->model()->GetTemplateURLs().size());
}

// Tests that a TemplateURL can't be edited if it is the managed default search
// provider.
TEST_F(KeywordEditorControllerManagedDSPTest, EditManagedDefault) {
  controller()->AddTemplateURL(kA, kB, "http://c{searchTerms}");
  controller()->AddTemplateURL(kA1, kB1, "http://d{searchTerms}");
  ClearChangeCount();

  const TemplateURL* turl1 = util()->model()->GetTemplateURLForKeyword(u"b");
  ASSERT_NE(turl1, nullptr);
  const TemplateURL* turl2 = util()->model()->GetTemplateURLForKeyword(u"b1");
  ASSERT_NE(turl2, nullptr);

  EXPECT_TRUE(controller()->CanEdit(turl1));
  EXPECT_TRUE(controller()->CanEdit(turl2));

  // Simulate setting a managed default.  This will add another template URL to
  // the model.
  SimulateDefaultSearchIsManaged(turl2->url(), /*is_mandatory=*/true);
  EXPECT_TRUE(util()->model()->is_default_search_managed());
  EXPECT_TRUE(controller()->CanEdit(turl1));
  EXPECT_TRUE(controller()->CanEdit(turl2));
  EXPECT_FALSE(
      controller()->CanEdit(util()->model()->GetDefaultSearchProvider()));
  EXPECT_TRUE(
      controller()->IsManaged(util()->model()->GetDefaultSearchProvider()));
}

// Tests that a `TemplateURL` can be edited if it is the recommended default
// search provider.
TEST_F(KeywordEditorControllerManagedDSPTest, EditRecommendedDefault) {
  controller()->AddTemplateURL(kA, kB, "http://c{searchTerms}");
  controller()->AddTemplateURL(kA1, kB1, "http://d{searchTerms}");
  ClearChangeCount();

  const TemplateURL* turl1 = util()->model()->GetTemplateURLForKeyword(u"b");
  ASSERT_NE(turl1, nullptr);
  const TemplateURL* turl2 = util()->model()->GetTemplateURLForKeyword(u"b1");
  ASSERT_NE(turl2, nullptr);

  EXPECT_TRUE(controller()->CanEdit(turl1));
  EXPECT_TRUE(controller()->CanEdit(turl2));

  // Simulate setting a recommended default. This will add another template URL
  // to the model.
  SimulateDefaultSearchIsManaged(turl2->url(), /*is_mandatory=*/false);
  EXPECT_EQ(kManaged,
            util()->model()->GetDefaultSearchProvider()->short_name());
  EXPECT_FALSE(util()->model()->is_default_search_managed());
  EXPECT_TRUE(controller()->CanEdit(turl1));
  EXPECT_TRUE(controller()->CanEdit(turl2));
  EXPECT_TRUE(
      controller()->CanEdit(util()->model()->GetDefaultSearchProvider()));
  EXPECT_FALSE(
      controller()->IsManaged(util()->model()->GetDefaultSearchProvider()));
}

TEST_F(KeywordEditorControllerNoWebDataTest, MakeDefaultNoWebData) {
  TemplateURLID id =
      controller()->AddTemplateURL(kA, kB, "http://c{searchTerms}");
  ClearChangeCount();

  // This should not result in a crash.
  controller()->MakeDefaultTemplateURL(
      id, search_engines::ChoiceMadeLocation::kOther);
  const TemplateURL* turl = util()->model()->GetTemplateURLForKeyword(kB);
  EXPECT_EQ(turl, util()->model()->GetDefaultSearchProvider());
}

// Mutates the TemplateURLService and make sure the `id_to_turl_` mapping is
// updating appropriately.
TEST_F(KeywordEditorControllerTest, MutateTemplateURLService) {
  TemplateURLData data;
  data.SetShortName(u"b");
  data.SetKeyword(u"a");
  TemplateURL* turl = util()->model()->Add(std::make_unique<TemplateURL>(data));
  TemplateURLID id = turl->id();
  ClearChangeCount();

  // Initially, the mapping should contain the added template URL.
  ASSERT_NE(nullptr, controller()->GetTemplateURL(id));

  // Remove the template URL from the TemplateURLService.
  util()->model()->Remove(turl);

  // TemplateURLService should have updated.
  VerifyChanged();

  // And should no longer contain the TemplateURL.
  EXPECT_EQ(nullptr, controller()->GetTemplateURL(id));
}

// TODO (crbug.com/494551138): Remove once `SearchSettingsUpdate` is launched.
class KeywordEditorControllerWithTableModelTest
    : public KeywordEditorControllerTest {
 public:
  KeywordEditorControllerWithTableModelTest() {
    scoped_feature_list_.InitWithFeatures({},
                                          {switches::kSearchSettingsUpdate});
  }

  KeywordEditorControllerWithTableModelTest(
      const KeywordEditorControllerWithTableModelTest&) = delete;
  KeywordEditorControllerWithTableModelTest& operator=(
      const KeywordEditorControllerWithTableModelTest&) = delete;

  ~KeywordEditorControllerWithTableModelTest() override = default;

  // Specifies examples for tests that verify ordering of search engines.
  struct SearchEngineOrderingTestCase {
    const char16_t* keyword;
    const char16_t* short_name;
    bool is_active;
    bool created_by_site_search_policy = false;
    bool created_by_search_aggregator_policy = false;
    bool featured_by_policy = false;
    bool safe_for_autoreplace = false;
  };

  void AddTestCases(
      const std::vector<SearchEngineOrderingTestCase>& kTestCases) {
    for (SearchEngineOrderingTestCase test_case : kTestCases) {
      util()->model()->Add(CreateTemplateUrlForSortingTest(test_case));
      // TemplateURLService should have updated.
      VerifyChanged();
    }
  }

  std::unique_ptr<TemplateURL> CreateTemplateUrlForSortingTest(
      SearchEngineOrderingTestCase test_case) {
    TemplateURLData data;
    data.SetKeyword(test_case.keyword);
    data.SetShortName(test_case.short_name);
    data.is_active = test_case.is_active
                         ? TemplateURLData::ActiveStatus::kTrue
                         : TemplateURLData::ActiveStatus::kFalse;
    data.policy_origin = test_case.created_by_search_aggregator_policy
                             ? TemplateURLData::PolicyOrigin::kSearchAggregator
                         : test_case.created_by_site_search_policy
                             ? TemplateURLData::PolicyOrigin::kSiteSearch
                             : TemplateURLData::PolicyOrigin::kNoPolicy;
    data.featured_by_policy = test_case.featured_by_policy;
    data.safe_for_autoreplace = test_case.safe_for_autoreplace;
    return std::make_unique<TemplateURL>(data);
  }

  void CheckKeywordsToDisplay(
      const std::vector<std::u16string>& kExpectedShortNamesOrder,
      const std::vector<std::u16string>& kExpectedKeywordsToDisplay) {
    const size_t numExpectedKeywords = kExpectedShortNamesOrder.size();

    ASSERT_EQ(table_model()->last_active_engine_index(),
              table_model()->last_search_engine_index() + numExpectedKeywords);
    ASSERT_EQ(table_model()->last_other_engine_index(),
              table_model()->last_active_engine_index());

    for (size_t i = 0; i < numExpectedKeywords; ++i) {
      const size_t row = table_model()->last_search_engine_index() + i;
      const TemplateURL* template_url = table_model()->GetTemplateURL(row);
      ASSERT_TRUE(template_url);
      EXPECT_EQ(template_url->short_name(), kExpectedShortNamesOrder[i]);
      EXPECT_EQ(base::i18n::GetDisplayStringInLTRDirectionality(
                    template_url->keyword()),
                kExpectedKeywordsToDisplay[i]);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(KeywordEditorControllerWithTableModelTest, EnginesSortedByName) {
  const std::vector<SearchEngineOrderingTestCase> kTestCases = {
      {
          .keyword = u"kw1",
          .short_name = u"Active 3",
          .is_active = true,
      },
      {
          .keyword = u"kw2",
          .short_name = u"Active 1",
          .is_active = true,
      },
      {
          .keyword = u"kw3",
          .short_name = u"inactive 1",
          .is_active = false,
      },
      {
          .keyword = u"kw4",
          .short_name = u"active 2",
          .is_active = true,
      },
      {
          .keyword = u"kw5",
          .short_name = u"Inactive 2",
          .is_active = false,
      },
  };

  const auto kExpectedShortNamesOrder = std::to_array<std::u16string>({
      u"Active 1",
      u"active 2",
      u"Active 3",
      u"inactive 1",
      u"Inactive 2",
  });

  AddTestCases(kTestCases);

  ASSERT_EQ(table_model()->last_active_engine_index(),
            table_model()->last_search_engine_index() + 3);
  ASSERT_EQ(table_model()->last_other_engine_index(),
            table_model()->last_active_engine_index() + 2);

  for (size_t i = 0; i < std::size(kExpectedShortNamesOrder); ++i) {
    const TemplateURL* template_url = table_model()->GetTemplateURL(
        table_model()->last_search_engine_index() + i);
    ASSERT_TRUE(template_url);
    EXPECT_EQ(template_url->short_name(), kExpectedShortNamesOrder[i]);
  }
}

TEST_F(KeywordEditorControllerWithTableModelTest,
       EnginesSortedByNameWithManagedSiteSearch) {
  const std::vector<SearchEngineOrderingTestCase> kTestCases = {
      {
          .keyword = u"kw1",
          .short_name = u"Non-managed 3",
          .is_active = true,
      },
      {
          .keyword = u"kw2",
          .short_name = u"Non-managed 1",
          .is_active = true,
      },
      {
          .keyword = u"kw3",
          .short_name = u"policy 1",
          .is_active = true,
          .created_by_site_search_policy = true,
      },
      {
          .keyword = u"kw4",
          .short_name = u"non-managed 2",
          .is_active = true,
      },
      {
          .keyword = u"kw5",
          .short_name = u"Policy 2",
          .is_active = true,
          .created_by_site_search_policy = true,
      },
  };

  const auto kExpectedShortNamesOrder = std::to_array<std::u16string>({
      u"policy 1",
      u"Policy 2",
      u"Non-managed 1",
      u"non-managed 2",
      u"Non-managed 3",
  });

  AddTestCases(kTestCases);

  ASSERT_EQ(table_model()->last_active_engine_index(),
            table_model()->last_search_engine_index() +
                std::size(kExpectedShortNamesOrder));
  ASSERT_EQ(table_model()->last_other_engine_index(),
            table_model()->last_active_engine_index());

  for (size_t i = 0; i < std::size(kExpectedShortNamesOrder); ++i) {
    const TemplateURL* template_url = table_model()->GetTemplateURL(
        table_model()->last_search_engine_index() + i);
    ASSERT_TRUE(template_url);
    EXPECT_EQ(template_url->short_name(), kExpectedShortNamesOrder[i]);
  }
}

TEST_F(KeywordEditorControllerWithTableModelTest,
       FeaturedEnterpriseSiteSearch) {
  const std::vector<SearchEngineOrderingTestCase> kTestCases = {
      {
          .keyword = u"@kw1",
          .short_name = u"Featured 1",
          .is_active = true,
          .created_by_site_search_policy = true,
          .featured_by_policy = true,
      },
      {
          .keyword = u"kw1",
          .short_name = u"Featured 1",
          .is_active = true,
          .created_by_site_search_policy = true,
          .featured_by_policy = false,
      },
      {
          .keyword = u"@kw2",
          .short_name = u"Featured 2",
          .is_active = true,
          .created_by_site_search_policy = true,
          .featured_by_policy = true,
      },
      {
          .keyword = u"kw2",
          .short_name = u"User-defined engine",
          .is_active = true,
          .created_by_site_search_policy = false,
      },
      {
          .keyword = u"kw3",
          .short_name = u"Non-featured",
          .is_active = true,
          .created_by_site_search_policy = true,
          .featured_by_policy = false,
      },
  };

  AddTestCases(kTestCases);

  const auto kExpectedShortNamesOrder = std::vector<std::u16string>({
      u"Featured 1",
      u"Featured 1",
      u"Featured 2",
      u"Non-featured",
      u"User-defined engine",
  });
  const auto kExpectedKeywordsToDisplay = std::vector<std::u16string>({
      u"@kw1",
      u"kw1",
      u"@kw2",
      u"kw3",
      u"kw2",
  });

  CheckKeywordsToDisplay(kExpectedShortNamesOrder, kExpectedKeywordsToDisplay);
}

TEST_F(KeywordEditorControllerWithTableModelTest,
       EnterpriseSiteSearchConflictWithExistingEngines) {
  const std::vector<SearchEngineOrderingTestCase> kTestCases = {
      {
          .keyword = u"kw1",
          .short_name = u"User-defined engine",
          .is_active = true,
          .created_by_site_search_policy = false,
          .safe_for_autoreplace = false,
      },
      {
          .keyword = u"@kw1",
          .short_name = u"User-defined engine with @",
          .is_active = true,
          .created_by_site_search_policy = false,
          .safe_for_autoreplace = false,
      },
      {
          .keyword = u"kw2",
          .short_name = u"Auto-created engine",
          .is_active = true,
          .created_by_site_search_policy = false,
          .featured_by_policy = false,
          .safe_for_autoreplace = true,
      },
      {
          .keyword = u"@kw2",
          .short_name = u"Auto-created engine with @",
          .is_active = true,
          .created_by_site_search_policy = false,
          .featured_by_policy = false,
          .safe_for_autoreplace = true,
      },
      {
          .keyword = u"@kw1",
          .short_name = u"Featured 1",
          .is_active = true,
          .created_by_site_search_policy = true,
          .featured_by_policy = true,
      },
      {
          .keyword = u"kw1",
          .short_name = u"Featured 1",
          .is_active = true,
          .created_by_site_search_policy = true,
          .featured_by_policy = false,
      },
      {
          .keyword = u"@kw2",
          .short_name = u"Featured 2",
          .is_active = true,
          .created_by_site_search_policy = true,
          .featured_by_policy = true,
      },
      {
          .keyword = u"kw2",
          .short_name = u"Featured 2",
          .is_active = true,
          .created_by_site_search_policy = true,
          .featured_by_policy = false,
      },
  };

  AddTestCases(kTestCases);

  const auto kExpectedShortNamesOrder = std::vector<std::u16string>({
      u"Featured 1",
      u"Featured 2",
      u"Featured 2",
      u"User-defined engine",
  });
  const auto kExpectedKeywordsToDisplay = std::vector<std::u16string>({
      u"@kw1",
      u"@kw2",
      u"kw2",
      u"kw1",
  });

  CheckKeywordsToDisplay(kExpectedShortNamesOrder, kExpectedKeywordsToDisplay);
}

TEST_F(KeywordEditorControllerWithTableModelTest, EnterpriseSearchAggregator) {
  const std::vector<SearchEngineOrderingTestCase> kTestCases = {
      {
          .keyword = u"@kw1",
          .short_name = u"Featured 1",
          .is_active = true,
          .created_by_search_aggregator_policy = true,
          .featured_by_policy = true,
      },
      {
          .keyword = u"kw1",
          .short_name = u"Featured 1",
          .is_active = true,
          .created_by_search_aggregator_policy = true,
          .featured_by_policy = false,
      },
      {
          .keyword = u"kw2",
          .short_name = u"Non-featured",
          .is_active = true,
          .created_by_site_search_policy = false,
          .featured_by_policy = false,
      },
  };

  AddTestCases(kTestCases);

  const auto kExpectedShortNamesOrder = std::vector<std::u16string>(
      {u"Featured 1", u"Featured 1", u"Non-featured"});
  const auto kExpectedKeywordsToDisplay =
      std::vector<std::u16string>({u"@kw1", u"kw1", u"kw2"});

  CheckKeywordsToDisplay(kExpectedShortNamesOrder, kExpectedKeywordsToDisplay);
}

TEST_F(KeywordEditorControllerWithTableModelTest,
       EnterpriseSearchAggregatorConflictWithExistingNonPolicyEngines) {
  const std::vector<SearchEngineOrderingTestCase> kTestCases = {
      {
          .keyword = u"kw1",
          .short_name = u"User-defined engine",
          .is_active = true,
          .created_by_search_aggregator_policy = false,
          .safe_for_autoreplace = false,
      },
      {
          .keyword = u"@kw1",
          .short_name = u"User-defined engine with @",
          .is_active = true,
          .created_by_search_aggregator_policy = false,
          .safe_for_autoreplace = false,
      },
      {
          .keyword = u"kw2",
          .short_name = u"Auto-created engine",
          .is_active = true,
          .created_by_search_aggregator_policy = false,
          .featured_by_policy = false,
          .safe_for_autoreplace = true,
      },
      {
          .keyword = u"@kw2",
          .short_name = u"Auto-created engine with @",
          .is_active = true,
          .created_by_search_aggregator_policy = false,
          .featured_by_policy = false,
          .safe_for_autoreplace = true,
      },
      {
          .keyword = u"@kw1",
          .short_name = u"Featured 1",
          .is_active = true,
          .created_by_search_aggregator_policy = true,
          .featured_by_policy = true,
      },
      {
          .keyword = u"kw1",
          .short_name = u"Featured 1",
          .is_active = true,
          .created_by_search_aggregator_policy = true,
          .featured_by_policy = false,
      },
  };

  AddTestCases(kTestCases);

  const auto kExpectedShortNamesOrder = std::vector<std::u16string>(
      {u"Featured 1", u"Auto-created engine", u"Auto-created engine with @",
       u"User-defined engine"});
  const auto kExpectedKeywordsToDisplay =
      std::vector<std::u16string>({u"@kw1", u"kw2", u"@kw2", u"kw1"});

  CheckKeywordsToDisplay(kExpectedShortNamesOrder, kExpectedKeywordsToDisplay);
}

TEST_F(KeywordEditorControllerWithTableModelTest,
       EnterpriseSiteSearchAndSearchAggregator) {
  const std::vector<SearchEngineOrderingTestCase> kTestCases = {
      {
          .keyword = u"@kw1",
          .short_name = u"Featured 1",
          .is_active = true,
          .created_by_site_search_policy = true,
          .featured_by_policy = true,
      },
      {
          .keyword = u"kw1",
          .short_name = u"Featured 1",
          .is_active = true,
          .created_by_site_search_policy = true,
          .featured_by_policy = false,
      },
      {
          .keyword = u"kw2",
          .short_name = u"Non-featured",
          .is_active = true,
          .created_by_site_search_policy = false,
          .featured_by_policy = false,
      },
      {
          .keyword = u"@kw3",
          .short_name = u"Featured 3",
          .is_active = true,
          .created_by_search_aggregator_policy = true,
          .featured_by_policy = true,
      },
      {
          .keyword = u"kw3",
          .short_name = u"Featured 3",
          .is_active = true,
          .created_by_search_aggregator_policy = true,
          .featured_by_policy = false,
      },
  };

  AddTestCases(kTestCases);

  const auto kExpectedShortNamesOrder =
      std::vector<std::u16string>({u"Featured 1", u"Featured 1", u"Featured 3",
                                   u"Featured 3", u"Non-featured"});
  const auto kExpectedKeywordsToDisplay =
      std::vector<std::u16string>({u"@kw1", u"kw1", u"@kw3", u"kw3", u"kw2"});

  CheckKeywordsToDisplay(kExpectedShortNamesOrder, kExpectedKeywordsToDisplay);
}
