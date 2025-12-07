// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/existing_comparison_table_sub_menu_model.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/commerce_utils.h"
#include "components/commerce/core/product_specifications/mock_product_specifications_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace commerce {
namespace {

const char kTestUrl1[] = "https://example1.com";
const char kTestUrl2[] = "https://example2.com";
const char kTestUrl3[] = "https://example3.com";

// One item for "New comparison table", one for the separator, and two for the
// existing tables.
const int kFullItemCount = 4;

}  // namespace

class ExistingComparisonTableSubMenuModelTest : public testing::Test {
 public:
  ExistingComparisonTableSubMenuModelTest() = default;

  void SetUp() override {
    features_.InitAndEnableFeature(commerce::kProductSpecifications);
    profile_ = std::make_unique<TestingProfile>();
    delegate_ = std::make_unique<TestTabStripModelDelegate>();
    tab_strip_model_ =
        std::make_unique<TabStripModel>(delegate_.get(), profile_.get());
    product_specifications_service_ =
        std::make_unique<MockProductSpecificationsService>();
    product_specifications_data_ = std::vector<ProductSpecificationsSet>{
        ProductSpecificationsSet(
            base::Uuid::GenerateRandomV4().AsLowercaseString(), 0, 0,
            {
                GURL(kTestUrl1),
            },
            "Set 1"),
        ProductSpecificationsSet(
            base::Uuid::GenerateRandomV4().AsLowercaseString(), 0, 0,
            {
                GURL(kTestUrl1),
                GURL(kTestUrl2),
            },
            "Set 2")};

    ON_CALL(*product_specifications_service_, GetAllProductSpecifications())
        .WillByDefault(testing::Return(product_specifications_data_));
  }

  void AddTab(GURL url) {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                          nullptr);
    content::WebContentsTester::For(web_contents.get())
        ->SetLastCommittedURL(url);

    tab_strip_model()->AppendTab(
        std::make_unique<tabs::TabModel>(std::move(web_contents),
                                         tab_strip_model()),
        /*foreground=*/true);
  }

  TabStripModel* tab_strip_model() { return tab_strip_model_.get(); }
  MockProductSpecificationsService* product_specifications_service() {
    return product_specifications_service_.get();
  }
  std::vector<ProductSpecificationsSet> product_specifications_data() {
    return product_specifications_data_;
  }

 private:
  content::BrowserTaskEnvironment task_environment;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  const tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
  base::test::ScopedFeatureList features_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TestTabStripModelDelegate> delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  std::unique_ptr<MockProductSpecificationsService>
      product_specifications_service_;
  std::vector<ProductSpecificationsSet> product_specifications_data_;
};

TEST_F(ExistingComparisonTableSubMenuModelTest, ShouldShowSubmenu_NoSets) {
  ON_CALL(*product_specifications_service(), GetAllProductSpecifications())
      .WillByDefault(testing::Return(std::vector<ProductSpecificationsSet>()));

  ASSERT_FALSE(ExistingComparisonTableSubMenuModel::ShouldShowSubmenu(
      GURL(kTestUrl3), product_specifications_service()));
}

TEST_F(ExistingComparisonTableSubMenuModelTest,
       ShouldShowSubmenu_NoSetsContainUrl) {
  ASSERT_TRUE(ExistingComparisonTableSubMenuModel::ShouldShowSubmenu(
      GURL(kTestUrl3), product_specifications_service()));
}

TEST_F(ExistingComparisonTableSubMenuModelTest,
       ShouldShowSubmenu_SomeSetsContainUrl) {
  ASSERT_TRUE(ExistingComparisonTableSubMenuModel::ShouldShowSubmenu(
      GURL(kTestUrl2), product_specifications_service()));
}

TEST_F(ExistingComparisonTableSubMenuModelTest,
       ShouldShowSubmenu_AllSetsContainUrl) {
  ASSERT_FALSE(ExistingComparisonTableSubMenuModel::ShouldShowSubmenu(
      GURL(kTestUrl1), product_specifications_service()));
}

TEST_F(ExistingComparisonTableSubMenuModelTest, BuildMenuItems_MultipleSets) {
  AddTab(GURL(kTestUrl3));
  tab_strip_model()->SelectTabAt(0);

  ExistingComparisonTableSubMenuModel sub_menu_model(
      nullptr, nullptr, tab_strip_model(), 0, product_specifications_service());

  ASSERT_TRUE(sub_menu_model.GetItemCount() == kFullItemCount);
  ASSERT_TRUE(sub_menu_model.GetLabelAt(2) == u"Set 1");
  ASSERT_TRUE(sub_menu_model.GetLabelAt(3) == u"Set 2");
}

TEST_F(ExistingComparisonTableSubMenuModelTest,
       BuildMenuItems_SomeSetsContainUrl) {
  AddTab(GURL(kTestUrl2));
  tab_strip_model()->SelectTabAt(0);

  ExistingComparisonTableSubMenuModel sub_menu_model(
      nullptr, nullptr, tab_strip_model(), 0, product_specifications_service());

  // New item, separator, and one table item.
  ASSERT_TRUE(sub_menu_model.GetItemCount() == kFullItemCount - 1);
  ASSERT_TRUE(sub_menu_model.GetLabelAt(2) == u"Set 1");
}

TEST_F(ExistingComparisonTableSubMenuModelTest, UrlAddedToExistingSet) {
  AddTab(GURL(kTestUrl3));
  tab_strip_model()->SelectTabAt(0);

  const auto& title = tab_strip_model()->GetWebContentsAt(0)->GetTitle();

  ExistingComparisonTableSubMenuModel sub_menu_model(
      nullptr, nullptr, tab_strip_model(), 0, product_specifications_service());
  ASSERT_TRUE(sub_menu_model.GetItemCount() == kFullItemCount);

  ON_CALL(*product_specifications_service(),
          GetSetByUuid(product_specifications_data()[0].uuid()))
      .WillByDefault(testing::Return(product_specifications_data()[0]));
  EXPECT_CALL(*product_specifications_service(),
              SetUrls(product_specifications_data()[0].uuid(),
                      testing::ElementsAre(UrlInfo(GURL(kTestUrl1), u""),
                                           UrlInfo(GURL(kTestUrl3), title))))
      .Times(1);

  sub_menu_model.ExecuteCommand(sub_menu_model.GetCommandIdAt(2), 0);
}

}  // namespace commerce
