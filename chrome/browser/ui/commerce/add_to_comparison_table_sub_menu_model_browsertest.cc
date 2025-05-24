// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commerce/add_to_comparison_table_sub_menu_model.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/commerce_utils.h"
#include "components/commerce/core/mojom/product_specifications.mojom.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/product_specifications/mock_product_specifications_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace commerce {
namespace {

const char kTestUrl1[] = "https://example1.com";
const char kTestUrl2[] = "https://example2.com";
const char kTestUrl3[] = "https://example3.com";

// One item for "New comparison table", one for the separator, and two for the
// existing tables.
const int kFullItemCount = 4;

}  // namespace

class AddToComparisonTableSubMenuModelBrowserTest
    : public InProcessBrowserTest {
 public:
  AddToComparisonTableSubMenuModelBrowserTest() {
    test_features_.InitAndEnableFeature(commerce::kProductSpecifications);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    browser()->profile()->GetPrefs()->SetInteger(
        commerce::kProductSpecificationsAcceptedDisclosureVersion,
        static_cast<int>(
            commerce::product_specifications::mojom::DisclosureVersion::kV1));

    product_specs_service =
        std::make_unique<MockProductSpecificationsService>();
    ON_CALL(*product_specs_service, GetAllProductSpecifications())
        .WillByDefault(testing::Return(kProductSpecsSets));
  }

  void NavigateToURL(const GURL& url) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

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

IN_PROC_BROWSER_TEST_F(AddToComparisonTableSubMenuModelBrowserTest,
                       ContainsMultipleSets) {
  AddToComparisonTableSubMenuModel sub_menu_model(browser(),
                                                  product_specs_service.get());

  ASSERT_TRUE(sub_menu_model.GetItemCount() == kFullItemCount);

  ASSERT_TRUE(sub_menu_model.GetLabelAt(2) == u"Set 1");
  ASSERT_TRUE(sub_menu_model.GetLabelAt(3) == u"Set 2");

  ASSERT_TRUE(
      sub_menu_model.IsCommandIdEnabled(sub_menu_model.GetCommandIdAt(0)));
  ASSERT_TRUE(
      sub_menu_model.IsCommandIdEnabled(sub_menu_model.GetCommandIdAt(2)));
  ASSERT_TRUE(
      sub_menu_model.IsCommandIdEnabled(sub_menu_model.GetCommandIdAt(3)));
}

IN_PROC_BROWSER_TEST_F(AddToComparisonTableSubMenuModelBrowserTest,
                       ContainsNoSets) {
  ON_CALL(*product_specs_service, GetAllProductSpecifications())
      .WillByDefault(testing::Return(std::vector<ProductSpecificationsSet>()));

  AddToComparisonTableSubMenuModel sub_menu_model(browser(),
                                                  product_specs_service.get());

  // One item for "New comparison table".
  ASSERT_TRUE(sub_menu_model.GetItemCount() == 1);

  ASSERT_TRUE(
      sub_menu_model.IsCommandIdEnabled(sub_menu_model.GetCommandIdAt(0)));
}

IN_PROC_BROWSER_TEST_F(AddToComparisonTableSubMenuModelBrowserTest,
                       DisablesTablesContainingCurrentUrl) {
  NavigateToURL(GURL(kTestUrl2));

  AddToComparisonTableSubMenuModel sub_menu_model(browser(),
                                                  product_specs_service.get());

  ASSERT_TRUE(sub_menu_model.GetItemCount() == kFullItemCount);

  // Set 2 should be disabled since it already contains the current URL.
  ASSERT_TRUE(
      sub_menu_model.IsCommandIdEnabled(sub_menu_model.GetCommandIdAt(2)));
  ASSERT_FALSE(
      sub_menu_model.IsCommandIdEnabled(sub_menu_model.GetCommandIdAt(3)));
}

IN_PROC_BROWSER_TEST_F(AddToComparisonTableSubMenuModelBrowserTest,
                       UrlAddedToNewSet) {
  NavigateToURL(GURL(kTestUrl1));

  AddToComparisonTableSubMenuModel sub_menu_model(browser(),
                                                  product_specs_service.get());

  sub_menu_model.ExecuteCommand(sub_menu_model.GetCommandIdAt(0), 0);

  // Check that a new tab was opened to create the table with the URL.
  ASSERT_TRUE(browser()->tab_strip_model()->count() == 2);
  ASSERT_TRUE(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL() ==
      GetProductSpecsTabUrl({GURL(kTestUrl1)}));
}

IN_PROC_BROWSER_TEST_F(AddToComparisonTableSubMenuModelBrowserTest,
                       UrlAddedToExistingSet) {
  NavigateToURL(GURL(kTestUrl3));
  const auto& title =
      browser()->GetActiveTabInterface()->GetContents()->GetTitle();

  AddToComparisonTableSubMenuModel sub_menu_model(browser(),
                                                  product_specs_service.get());

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
