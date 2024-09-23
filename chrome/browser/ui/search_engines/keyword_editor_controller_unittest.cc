// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/search_engines/keyword_editor_controller.h"

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/browser/ui/search_engines/template_url_table_model.h"
#include "chrome/test/base/testing_profile.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/choice_made_location.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/table_model_observer.h"

using base::ASCIIToUTF16;

static const std::u16string kA(u"a");
static const std::u16string kA1(u"a1");
static const std::u16string kB(u"b");
static const std::u16string kB1(u"b1");
static const std::u16string kManaged(u"managed");

// Base class for keyword editor tests. Creates a profile containing an
// empty TemplateURLService.
class KeywordEditorControllerTest : public testing::Test,
                                    public ui::TableModelObserver {
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
    if (simulate_load_failure_)
      util_.model()->OnWebDataServiceRequestDone(0, nullptr);
    else
      util_.VerifyLoad();

    controller_ = std::make_unique<KeywordEditorController>(&profile_);
    controller_->table_model()->SetObserver(this);
  }

  void TearDown() override { controller_.reset(); }

  void OnModelChanged() override { model_changed_count_++; }

  void OnItemsChanged(size_t start, size_t length) override {}

  void OnItemsAdded(size_t start, size_t length) override {}

  void OnItemsRemoved(size_t start, size_t length) override {}

  void VerifyChanged() {
    ASSERT_EQ(1, model_changed_count_);
    ClearChangeCount();
  }

  void ClearChangeCount() { model_changed_count_ = 0; }

  void SimulateDefaultSearchIsManaged(const std::string& url,
                                      bool is_mandatory) {
    TemplateURLData managed_engine;
    managed_engine.SetShortName(kManaged);
    managed_engine.SetKeyword(kManaged);
    managed_engine.SetURL(url);
    managed_engine.created_by_policy =
        TemplateURLData::CreatedByPolicy::kDefaultSearchProvider;
    managed_engine.enforced_by_policy = is_mandatory;
    is_mandatory
        ? SetManagedDefaultSearchPreferences(managed_engine, true, &profile_)
        : SetRecommendedDefaultSearchPreferences(managed_engine, true,
                                                 &profile_);
  }

  TemplateURLTableModel* table_model() { return controller_->table_model(); }
  KeywordEditorController* controller() { return controller_.get(); }
  const TemplateURLServiceFactoryTestUtil* util() const { return &util_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<KeywordEditorController> controller_;
  TemplateURLServiceFactoryTestUtil util_;
  bool simulate_load_failure_;

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
  size_t original_row_count = table_model()->RowCount();
  controller()->AddTemplateURL(kA, kB, "http://c");

  // Verify the observer was notified.
  VerifyChanged();
  if (HasFatalFailure())
    return;

  // Verify the TableModel has the new data.
  ASSERT_EQ(original_row_count + 1, table_model()->RowCount());

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
}

// Tests making a TemplateURL the default search provider.
TEST_F(KeywordEditorControllerTest, MakeDefault) {
  int index = controller()->AddTemplateURL(kA, kB, "http://c{searchTerms}");
  ClearChangeCount();

  const TemplateURL* turl = util()->model()->GetTemplateURLForKeyword(kB);
  controller()->MakeDefaultTemplateURL(
      index, search_engines::ChoiceMadeLocation::kOther);
  // Making an item the default sends a handful of changes. Which are sent isn't
  // important, what is important is 'something' is sent.
  VerifyChanged();
  ASSERT_EQ(turl, util()->model()->GetDefaultSearchProvider());

  // Making it default a second time should fail.
  controller()->MakeDefaultTemplateURL(
      index, search_engines::ChoiceMadeLocation::kOther);
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

  int index = controller()->AddTemplateURL(kA1, kB1, "http://d{searchTerms}");
  ClearChangeCount();
  const TemplateURL* turl2 = util()->model()->GetTemplateURLForKeyword(kB1);
  ASSERT_NE(turl2, nullptr);
  EXPECT_TRUE(controller()->CanMakeDefault(turl2));

  // Update the default search provider.
  EXPECT_NE(turl2, util()->model()->GetDefaultSearchProvider());
  controller()->MakeDefaultTemplateURL(
      index, search_engines::ChoiceMadeLocation::kOther);
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
  int index = controller()->AddTemplateURL(kA, kB, "http://c{searchTerms}");
  ClearChangeCount();

  // This should not result in a crash.
  controller()->MakeDefaultTemplateURL(
      index, search_engines::ChoiceMadeLocation::kOther);
  const TemplateURL* turl = util()->model()->GetTemplateURLForKeyword(kB);
  EXPECT_EQ(turl, util()->model()->GetDefaultSearchProvider());
}

// Mutates the TemplateURLService and make sure table model is updating
// appropriately.
TEST_F(KeywordEditorControllerTest, MutateTemplateURLService) {
  size_t original_row_count = table_model()->RowCount();

  TemplateURLData data;
  data.SetShortName(u"b");
  data.SetKeyword(u"a");
  TemplateURL* turl = util()->model()->Add(std::make_unique<TemplateURL>(data));

  // Table model should have updated.
  VerifyChanged();

  // And should contain the newly added TemplateURL.
  ASSERT_EQ(original_row_count + 1, table_model()->RowCount());
  ASSERT_TRUE(table_model()->IndexOfTemplateURL(turl).has_value());
}

// Specifies examples for tests that verify ordering of search engines.
struct SearchEngineOrderingTestCase {
  const char16_t* keyword;
  const char16_t* short_name;
  bool is_active;
  bool created_by_site_search_policy = false;
  bool featured_by_policy = false;
  bool safe_for_autoreplace = false;
};

std::unique_ptr<TemplateURL> CreateTemplateUrlForSortingTest(
    SearchEngineOrderingTestCase test_case) {
  TemplateURLData data;
  data.SetKeyword(test_case.keyword);
  data.SetShortName(test_case.short_name);
  data.is_active = test_case.is_active ? TemplateURLData::ActiveStatus::kTrue
                                       : TemplateURLData::ActiveStatus::kFalse;
  data.created_by_policy = test_case.created_by_site_search_policy
                               ? TemplateURLData::CreatedByPolicy::kSiteSearch
                               : TemplateURLData::CreatedByPolicy::kNoPolicy;
  data.featured_by_policy = test_case.featured_by_policy;
  data.safe_for_autoreplace = test_case.safe_for_autoreplace;
  return std::make_unique<TemplateURL>(data);
}

TEST_F(KeywordEditorControllerTest, EnginesSortedByName) {
  const SearchEngineOrderingTestCase kTestCases[] = {
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

  const std::u16string kExpectedShortNamesOrder[] = {
      u"Active 1", u"active 2", u"Active 3", u"inactive 1", u"Inactive 2"};

  std::vector<TemplateURL*> engines;
  for (SearchEngineOrderingTestCase test_case : kTestCases) {
    engines.push_back(
        util()->model()->Add(CreateTemplateUrlForSortingTest(test_case)));
    // Table model should have updated.
    VerifyChanged();
  }

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

TEST_F(KeywordEditorControllerTest, EnginesSortedByNameWithManagedSiteSearch) {
  const SearchEngineOrderingTestCase kTestCases[] = {
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

  const std::u16string kExpectedShortNamesOrder[] = {
      u"policy 1", u"Policy 2", u"Non-managed 1", u"non-managed 2",
      u"Non-managed 3"};

  std::vector<TemplateURL*> engines;
  for (SearchEngineOrderingTestCase test_case : kTestCases) {
    engines.push_back(
        util()->model()->Add(CreateTemplateUrlForSortingTest(test_case)));
    // Table model should have updated.
    VerifyChanged();
  }

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

TEST_F(KeywordEditorControllerTest, FeaturedEnterpriseSiteSearch) {
  const SearchEngineOrderingTestCase kTestCases[] = {
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

  const std::u16string kExpectedShortNamesOrder[] = {
      u"Featured 1", u"Featured 2", u"Non-featured", u"User-defined engine"};
  const std::u16string kExpectedKeywordsToDisplay[] = {u"@kw1, kw1", u"@kw2",
                                                       u"kw3", u"kw2"};

  std::vector<TemplateURL*> engines;
  for (SearchEngineOrderingTestCase test_case : kTestCases) {
    engines.push_back(
        util()->model()->Add(CreateTemplateUrlForSortingTest(test_case)));
    // Table model should have updated.
    VerifyChanged();
  }

  ASSERT_EQ(table_model()->last_active_engine_index(),
            table_model()->last_search_engine_index() +
                std::size(kExpectedShortNamesOrder));
  ASSERT_EQ(table_model()->last_other_engine_index(),
            table_model()->last_active_engine_index());

  for (size_t i = 0; i < std::size(kExpectedShortNamesOrder); ++i) {
    size_t row = table_model()->last_search_engine_index() + i;
    const TemplateURL* template_url = table_model()->GetTemplateURL(row);
    ASSERT_TRUE(template_url);
    EXPECT_EQ(template_url->short_name(), kExpectedShortNamesOrder[i]);
    EXPECT_EQ(table_model()->GetKeywordToDisplay(row),
              kExpectedKeywordsToDisplay[i]);
  }
}

TEST_F(KeywordEditorControllerTest,
       EnterpriseSiteSearchConflictWithExistingEngines) {
  const SearchEngineOrderingTestCase kTestCases[] = {
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

  const std::u16string kExpectedShortNamesOrder[] = {
      u"Featured 1", u"Featured 2", u"User-defined engine"};
  const std::u16string kExpectedKeywordsToDisplay[] = {u"@kw1", u"@kw2, kw2",
                                                       u"kw1"};

  std::vector<TemplateURL*> engines;
  for (SearchEngineOrderingTestCase test_case : kTestCases) {
    engines.push_back(
        util()->model()->Add(CreateTemplateUrlForSortingTest(test_case)));
    // Table model should have updated.
    VerifyChanged();
  }

  ASSERT_EQ(table_model()->last_active_engine_index(),
            table_model()->last_search_engine_index() +
                std::size(kExpectedShortNamesOrder));
  ASSERT_EQ(table_model()->last_other_engine_index(),
            table_model()->last_active_engine_index());

  for (size_t i = 0; i < std::size(kExpectedShortNamesOrder); ++i) {
    size_t row = table_model()->last_search_engine_index() + i;
    const TemplateURL* template_url = table_model()->GetTemplateURL(row);
    ASSERT_TRUE(template_url);
    EXPECT_EQ(template_url->short_name(), kExpectedShortNamesOrder[i]);
    EXPECT_EQ(table_model()->GetKeywordToDisplay(row),
              kExpectedKeywordsToDisplay[i]);
  }
}
