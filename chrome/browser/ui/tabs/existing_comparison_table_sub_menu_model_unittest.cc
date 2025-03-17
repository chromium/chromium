// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/existing_comparison_table_sub_menu_model.h"

#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/commerce_utils.h"
#include "components/commerce/core/mojom/product_specifications.mojom.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/product_specifications/mock_product_specifications_service.h"

namespace commerce {
namespace {

const char kTestUrl1[] = "https://example1.com";
const char kTestUrl2[] = "https://example2.com";
const char kTestUrl3[] = "https://example3.com";

// One item for "New comparison table", one for the separator, and two for the
// existing tables.
const int kFullItemCount = 4;

}  // namespace

class ExistingComparisonTableSubMenuModelTest
    : public BrowserWithTestWindowTest {
 public:
  ExistingComparisonTableSubMenuModelTest() {
    test_features_.InitAndEnableFeature(commerce::kProductSpecifications);
  }

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    product_specs_service =
        std::make_unique<MockProductSpecificationsService>();
    ON_CALL(*product_specs_service, GetAllProductSpecifications())
        .WillByDefault(testing::Return(kProductSpecsSets));
  }

  TabStripModel* tab_strip() { return browser()->tab_strip_model(); }

 protected:
  base::test::ScopedFeatureList test_features_;
  std::unique_ptr<MockProductSpecificationsService> product_specs_service;

  const std::vector<ProductSpecificationsSet> kProductSpecsSets = {
      ProductSpecificationsSet(
          base::Uuid::GenerateRandomV4().AsLowercaseString(),
          0,
          0,
          {
              GURL(kTestUrl1),
          },
          "Set 1"),
      ProductSpecificationsSet(
          base::Uuid::GenerateRandomV4().AsLowercaseString(),
          0,
          0,
          {
              GURL(kTestUrl1),
              GURL(kTestUrl2),
          },
          "Set 2")};
};

TEST_F(ExistingComparisonTableSubMenuModelTest, ShouldShowSubmenu_NoSets) {
  ON_CALL(*product_specs_service, GetAllProductSpecifications())
      .WillByDefault(testing::Return(std::vector<ProductSpecificationsSet>()));

  ASSERT_FALSE(ExistingComparisonTableSubMenuModel::ShouldShowSubmenu(
      GURL(kTestUrl3), product_specs_service.get()));
}

TEST_F(ExistingComparisonTableSubMenuModelTest,
       ShouldShowSubmenu_NoSetsContainUrl) {
  ASSERT_TRUE(ExistingComparisonTableSubMenuModel::ShouldShowSubmenu(
      GURL(kTestUrl3), product_specs_service.get()));
}

TEST_F(ExistingComparisonTableSubMenuModelTest,
       ShouldShowSubmenu_SomeSetsContainUrl) {
  ASSERT_TRUE(ExistingComparisonTableSubMenuModel::ShouldShowSubmenu(
      GURL(kTestUrl2), product_specs_service.get()));
}

TEST_F(ExistingComparisonTableSubMenuModelTest,
       ShouldShowSubmenu_AllSetsContainUrl) {
  ASSERT_FALSE(ExistingComparisonTableSubMenuModel::ShouldShowSubmenu(
      GURL(kTestUrl1), product_specs_service.get()));
}

TEST_F(ExistingComparisonTableSubMenuModelTest, BuildMenuItems_MultipleSets) {
  AddTab(browser(), GURL(kTestUrl3));
  tab_strip()->ToggleSelectionAt(0);

  ExistingComparisonTableSubMenuModel sub_menu_model(
      nullptr, nullptr, tab_strip(), 0, product_specs_service.get());

  ASSERT_TRUE(sub_menu_model.GetItemCount() == kFullItemCount);
  ASSERT_TRUE(sub_menu_model.GetLabelAt(2) == u"Set 1");
  ASSERT_TRUE(sub_menu_model.GetLabelAt(3) == u"Set 2");
}

TEST_F(ExistingComparisonTableSubMenuModelTest,
       BuildMenuItems_SomeSetsContainUrl) {
  AddTab(browser(), GURL(kTestUrl2));
  tab_strip()->ToggleSelectionAt(0);

  ExistingComparisonTableSubMenuModel sub_menu_model(
      nullptr, nullptr, tab_strip(), 0, product_specs_service.get());

  // New item, separator, and one table item.
  ASSERT_TRUE(sub_menu_model.GetItemCount() == kFullItemCount - 1);
  ASSERT_TRUE(sub_menu_model.GetLabelAt(2) == u"Set 1");
}

TEST_F(ExistingComparisonTableSubMenuModelTest, UrlAddedToExistingSet) {
  AddTab(browser(), GURL(kTestUrl3));
  tab_strip()->ToggleSelectionAt(0);

  const auto& title = tab_strip()->GetWebContentsAt(0)->GetTitle();

  ExistingComparisonTableSubMenuModel sub_menu_model(
      nullptr, nullptr, tab_strip(), 0, product_specs_service.get());
  ASSERT_TRUE(sub_menu_model.GetItemCount() == kFullItemCount);

  ON_CALL(*product_specs_service, GetSetByUuid(kProductSpecsSets[0].uuid()))
      .WillByDefault(testing::Return(kProductSpecsSets[0]));
  EXPECT_CALL(*product_specs_service,
              SetUrls(kProductSpecsSets[0].uuid(),
                      testing::ElementsAre(UrlInfo(GURL(kTestUrl1), u""),
                                           UrlInfo(GURL(kTestUrl3), title))))
      .Times(1);

  sub_menu_model.ExecuteCommand(sub_menu_model.GetCommandIdAt(2), 0);
}

}  // namespace commerce
