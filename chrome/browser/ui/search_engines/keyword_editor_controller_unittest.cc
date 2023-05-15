// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_engines/keyword_editor_controller.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/browser/ui/search_engines/template_url_table_model.h"
#include "chrome/test/base/testing_profile.h"
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
  controller()->MakeDefaultTemplateURL(index);
  // Making an item the default sends a handful of changes. Which are sent isn't
  // important, what is important is 'something' is sent.
  VerifyChanged();
  ASSERT_EQ(turl, util()->model()->GetDefaultSearchProvider());

  // Making it default a second time should fail.
  controller()->MakeDefaultTemplateURL(index);
  EXPECT_EQ(turl, util()->model()->GetDefaultSearchProvider());
}

// Tests that a TemplateURL can't be made the default if the default search
// provider is managed via policy.
TEST_F(KeywordEditorControllerTest, CannotSetDefaultWhileManaged) {
  controller()->AddTemplateURL(kA, kB, "http://c{searchTerms}");
  controller()->AddTemplateURL(kA1, kB1, "http://d{searchTerms}");
  ClearChangeCount();

  const TemplateURL* turl1 = util()->model()->GetTemplateURLForKeyword(u"b");
  ASSERT_NE(turl1, nullptr);
  const TemplateURL* turl2 = util()->model()->GetTemplateURLForKeyword(u"b1");
  ASSERT_NE(turl2, nullptr);

  EXPECT_TRUE(controller()->CanMakeDefault(turl1));
  EXPECT_TRUE(controller()->CanMakeDefault(turl2));

  SimulateDefaultSearchIsManaged(turl2->url(), /*is_mandatory=*/true);
  EXPECT_TRUE(util()->model()->is_default_search_managed());

  EXPECT_FALSE(controller()->CanMakeDefault(turl1));
  EXPECT_FALSE(controller()->CanMakeDefault(turl2));
}

// Tests that a TemplateURL can be made the default if the default search
// provider is recommended via policy.
TEST_F(KeywordEditorControllerTest, SetDefaultWhileRecommended) {
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

  int index = controller()->AddTemplateURL(kA1, kB1, "http://d{searchTerms}");
  ClearChangeCount();
  const TemplateURL* turl2 = util()->model()->GetTemplateURLForKeyword(kB1);
  ASSERT_NE(turl2, nullptr);
  EXPECT_TRUE(controller()->CanMakeDefault(turl2));

  // Update the default search provider.
  EXPECT_NE(turl2, util()->model()->GetDefaultSearchProvider());
  controller()->MakeDefaultTemplateURL(index);
  VerifyChanged();
  EXPECT_EQ(turl2, util()->model()->GetDefaultSearchProvider());

  // Ensure that the recommended search provider is not deleted.
  ASSERT_NE(util()->model()->GetTemplateURLForKeyword(kManaged), nullptr);
}

// Tests that a recomended search provider does not persist when a different
// recommended provider is applied via policy.
TEST_F(KeywordEditorControllerTest, UpdateRecommended) {
  // Simulate setting a recommended default provider.
  SimulateDefaultSearchIsManaged("url1", /*is_mandatory=*/false);
  EXPECT_EQ(kManaged,
            util()->model()->GetDefaultSearchProvider()->short_name());
  EXPECT_EQ("url1", util()->model()->GetDefaultSearchProvider()->url());
  EXPECT_FALSE(util()->model()->is_default_search_managed());
  auto original_size = util()->model()->GetTemplateURLs().size();

  // Update the default search provider to a different recommended provider.
  SimulateDefaultSearchIsManaged("url2", /*is_mandatory=*/false);
  EXPECT_EQ("url2", util()->model()->GetDefaultSearchProvider()->url());
  EXPECT_FALSE(util()->model()->is_default_search_managed());
  EXPECT_FALSE(
      util()->model()->GetDefaultSearchProvider()->enforced_by_policy());
  EXPECT_EQ(original_size, util()->model()->GetTemplateURLs().size());
}

// Tests that a recomended search provider does not persist when a managed
// provider is applied via policy.
TEST_F(KeywordEditorControllerTest, SetManagedWhileRecommended) {
  // Simulate setting a recommended default provider.
  SimulateDefaultSearchIsManaged("url1", /*is_mandatory=*/false);
  EXPECT_EQ(kManaged,
            util()->model()->GetDefaultSearchProvider()->short_name());
  EXPECT_EQ("url1", util()->model()->GetDefaultSearchProvider()->url());
  EXPECT_FALSE(util()->model()->is_default_search_managed());
  auto original_size = util()->model()->GetTemplateURLs().size();

  // Update the default search provider to a managed (enforced) provider.
  SimulateDefaultSearchIsManaged("url2", /*is_mandatory=*/true);
  EXPECT_EQ("url2", util()->model()->GetDefaultSearchProvider()->url());
  EXPECT_TRUE(util()->model()->is_default_search_managed());
  EXPECT_TRUE(
      util()->model()->GetDefaultSearchProvider()->enforced_by_policy());
  EXPECT_EQ(original_size, util()->model()->GetTemplateURLs().size());
}

// Tests that a TemplateURL can't be edited if it is the managed default search
// provider.
TEST_F(KeywordEditorControllerTest, EditManagedDefault) {
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
}

// Tests that a `TemplateURL` can be edited if it is the recommended default
// search provider.
TEST_F(KeywordEditorControllerTest, EditRecommendedDefault) {
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
}

TEST_F(KeywordEditorControllerNoWebDataTest, MakeDefaultNoWebData) {
  int index = controller()->AddTemplateURL(kA, kB, "http://c{searchTerms}");
  ClearChangeCount();

  // This should not result in a crash.
  controller()->MakeDefaultTemplateURL(index);
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
