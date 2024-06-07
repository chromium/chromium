// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commerce/product_specifications_entry_point_controller.h"

#include "base/uuid.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/commerce/core/commerce_utils.h"
#include "components/commerce/core/mock_cluster_manager.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/product_specifications/mock_product_specifications_service.h"
#include "components/commerce/core/product_specifications/product_specifications_service.h"
#include "components/commerce/core/product_specifications/product_specifications_set.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kTitle[] = "test_tile";
const char kTestUrl1[] = "chrome://new-tab-page/";
const char kTestUrl2[] = "chrome://version/";
const char kTestUrl3[] = "chrome://flags/";
const char kTestUrl4[] = "chrome://management/";

static const int64_t kProductId1 = 1;
static const int64_t kProductId2 = 2;
static const int64_t kProductId3 = 3;
static const int64_t kProductId4 = 4;
}  // namespace

class MockObserver
    : public commerce::ProductSpecificationsEntryPointController::Observer {
 public:
  MOCK_METHOD(void,
              ShowEntryPointWithTitle,
              (const std::string title),
              (override));
  MOCK_METHOD(void, HideEntryPoint, (), (override));
};

class ProductSpecificationsEntryPointControllerBrowserTest
    : public InProcessBrowserTest {
 public:
  ProductSpecificationsEntryPointControllerBrowserTest() = default;

  void SetUpOnMainThread() override {
    mock_shopping_service_ = static_cast<commerce::MockShoppingService*>(
        commerce::ShoppingServiceFactory::GetForBrowserContext(
            browser()->profile()));
    mock_cluster_manager_ = static_cast<commerce::MockClusterManager*>(
        mock_shopping_service_->GetClusterManager());
    mock_product_spec_service_ =
        static_cast<commerce::MockProductSpecificationsService*>(
            mock_shopping_service_->GetProductSpecificationsService());
    controller_ = browser()
                      ->browser_window_features()
                      ->product_specifications_entry_point_controller();
    observer_ = std::make_unique<MockObserver>();
    controller_->AddObserver(observer_.get());
    // This is needed to make sure that the URL changes caused by navigations
    // will happen immediately.
    browser()->set_update_ui_immediately_for_testing();
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &ProductSpecificationsEntryPointControllerBrowserTest::
                    OnWillCreateBrowserContextServices,
                weak_ptr_factory_.GetWeakPtr()));
  }

  void TearDownInProcessBrowserTestFixture() override {
    is_browser_context_services_created = false;
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    is_browser_context_services_created = true;
    commerce::ShoppingServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context) {
          return commerce::MockShoppingService::Build();
        }));
  }

 protected:
  raw_ptr<commerce::MockShoppingService, AcrossTasksDanglingUntriaged>
      mock_shopping_service_;
  raw_ptr<commerce::MockClusterManager, AcrossTasksDanglingUntriaged>
      mock_cluster_manager_;
  raw_ptr<commerce::MockProductSpecificationsService,
          AcrossTasksDanglingUntriaged>
      mock_product_spec_service_;
  raw_ptr<commerce::ProductSpecificationsEntryPointController,
          AcrossTasksDanglingUntriaged>
      controller_;
  base::CallbackListSubscription create_services_subscription_;
  std::unique_ptr<MockObserver> observer_;
  bool is_browser_context_services_created{false};

 private:
  base::WeakPtrFactory<ProductSpecificationsEntryPointControllerBrowserTest>
      weak_ptr_factory_{this};
};

IN_PROC_BROWSER_TEST_F(ProductSpecificationsEntryPointControllerBrowserTest,
                       TriggerEntryPointWithSelection) {
  // Mock EntryPointInfo returned by ClusterManager.
  std::map<GURL, uint64_t> similar_products = {{GURL(kTestUrl1), kProductId1},
                                               {GURL(kTestUrl2), kProductId2}};
  auto info =
      std::make_optional<commerce::EntryPointInfo>(kTitle, similar_products);
  mock_cluster_manager_->SetResponseForGetEntryPointInfoForSelection(info);

  // Set up observer.
  EXPECT_CALL(*observer_, ShowEntryPointWithTitle(kTitle)).Times(1);

  // Create two tabs and simulate selection.
  ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 0, GURL(kTestUrl1),
                                     ui::PAGE_TRANSITION_LINK, true));
  ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 1, GURL(kTestUrl2),
                                     ui::PAGE_TRANSITION_LINK, true));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(controller_->entry_point_info_for_testing().has_value());

  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kMouse));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(controller_->entry_point_info_for_testing().has_value());
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsEntryPointControllerBrowserTest,
                       TriggerEntryPointWithSelection_NotShowForSameProduct) {
  // Mock EntryPointInfo returned by ClusterManager which contains two products
  // with the same product ID.
  std::map<GURL, uint64_t> similar_products = {{GURL(kTestUrl1), kProductId1},
                                               {GURL(kTestUrl2), kProductId1}};
  auto info =
      std::make_optional<commerce::EntryPointInfo>(kTitle, similar_products);
  mock_cluster_manager_->SetResponseForGetEntryPointInfoForSelection(info);

  // Set up observer.
  EXPECT_CALL(*observer_, ShowEntryPointWithTitle(kTitle)).Times(0);

  // Create two tabs and simulate selection.
  ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 0, GURL(kTestUrl1),
                                     ui::PAGE_TRANSITION_LINK, true));
  ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 1, GURL(kTestUrl2),
                                     ui::PAGE_TRANSITION_LINK, true));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(controller_->entry_point_info_for_testing().has_value());

  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kMouse));
  base::RunLoop().RunUntilIdle();
  // Not trigger entry point because the two products have the same product ID.
  ASSERT_FALSE(controller_->entry_point_info_for_testing().has_value());
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsEntryPointControllerBrowserTest,
                       TriggerEntryPointWithNavigation) {
  // Mock EntryPointInfo returned by ClusterManager.
  std::map<GURL, uint64_t> similar_products = {{GURL(kTestUrl2), kProductId2},
                                               {GURL(kTestUrl3), kProductId3},
                                               {GURL(kTestUrl4), kProductId4}};
  auto info =
      std::make_optional<commerce::EntryPointInfo>(kTitle, similar_products);
  mock_cluster_manager_->SetResponseForGetEntryPointInfoForNavigation(info);

  // Set up observer.
  EXPECT_CALL(*observer_, ShowEntryPointWithTitle(kTitle)).Times(1);

  // Current window has to have more than three unique tabs that are similar in
  // order to trigger the entry point for navigation.
  std::vector<std::string> urls_to_open = {kTestUrl2, kTestUrl3, kTestUrl3,
                                           kTestUrl1};
  for (auto& url : urls_to_open) {
    ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 0, GURL(url),
                                       ui::PAGE_TRANSITION_LINK, true));
    base::RunLoop().RunUntilIdle();
    controller_->OnClusterFinishedForNavigation(GURL(url));
    base::RunLoop().RunUntilIdle();
    ASSERT_FALSE(controller_->entry_point_info_for_testing().has_value());
  }

  ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 0, GURL(kTestUrl4),
                                     ui::PAGE_TRANSITION_LINK, true));
  base::RunLoop().RunUntilIdle();
  controller_->OnClusterFinishedForNavigation(GURL(kTestUrl4));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(controller_->entry_point_info_for_testing().has_value());
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsEntryPointControllerBrowserTest,
                       TriggerEntryPointWithNavigation_NotShowForSameProduct) {
  // Mock EntryPointInfo returned by ClusterManager which contains two products
  // with the same product ID.
  std::map<GURL, uint64_t> similar_products = {{GURL(kTestUrl2), kProductId2},
                                               {GURL(kTestUrl3), kProductId2},
                                               {GURL(kTestUrl4), kProductId4}};
  auto info =
      std::make_optional<commerce::EntryPointInfo>(kTitle, similar_products);
  mock_cluster_manager_->SetResponseForGetEntryPointInfoForNavigation(info);

  // Set up observer.
  EXPECT_CALL(*observer_, ShowEntryPointWithTitle(kTitle)).Times(0);

  // Current window has to have more than three unique and different products
  // that are similar in order to trigger the entry point for navigation.
  std::vector<std::string> urls_to_open = {kTestUrl2, kTestUrl3, kTestUrl3,
                                           kTestUrl1};
  for (auto& url : urls_to_open) {
    ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 0, GURL(url),
                                       ui::PAGE_TRANSITION_LINK, true));
    base::RunLoop().RunUntilIdle();
    controller_->OnClusterFinishedForNavigation(GURL(url));
    base::RunLoop().RunUntilIdle();
    ASSERT_FALSE(controller_->entry_point_info_for_testing().has_value());
  }

  ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 0, GURL(kTestUrl4),
                                     ui::PAGE_TRANSITION_LINK, true));
  base::RunLoop().RunUntilIdle();
  controller_->OnClusterFinishedForNavigation(GURL(kTestUrl4));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(controller_->entry_point_info_for_testing().has_value());
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsEntryPointControllerBrowserTest,
                       HideEntryPoint) {
  // Trigger entry point with selection.
  std::map<GURL, uint64_t> similar_products = {{GURL(kTestUrl1), kProductId1},
                                               {GURL(kTestUrl2), kProductId2}};
  auto info =
      std::make_optional<commerce::EntryPointInfo>(kTitle, similar_products);
  mock_cluster_manager_->SetResponseForGetEntryPointInfoForSelection(info);
  ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 0, GURL(kTestUrl1),
                                     ui::PAGE_TRANSITION_LINK, true));
  ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 1, GURL(kTestUrl2),
                                     ui::PAGE_TRANSITION_LINK, true));
  base::RunLoop().RunUntilIdle();
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kMouse));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(controller_->entry_point_info_for_testing().has_value());

  // Reset EntryPointInfo when entry point has hidden.
  controller_->OnEntryPointHidden();
  ASSERT_FALSE(controller_->entry_point_info_for_testing().has_value());
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsEntryPointControllerBrowserTest,
                       ExecuteEntryPoint) {
  // Set up product spec service.
  EXPECT_CALL(*mock_product_spec_service_,
              AddProductSpecificationsSet(testing::_, testing::_))
      .Times(1);
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();
  commerce::ProductSpecificationsSet set(
      uuid.AsLowercaseString(), 0, 0, {GURL(kTestUrl1), GURL(kTestUrl2)}, "");
  ON_CALL(*mock_product_spec_service_, AddProductSpecificationsSet)
      .WillByDefault(testing::Return(set));

  // Trigger entry point with selection.
  std::map<GURL, uint64_t> similar_products = {{GURL(kTestUrl1), kProductId1},
                                               {GURL(kTestUrl2), kProductId2}};
  auto info =
      std::make_optional<commerce::EntryPointInfo>(kTitle, similar_products);
  mock_cluster_manager_->SetResponseForGetEntryPointInfoForSelection(info);
  ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 0, GURL(kTestUrl1),
                                     ui::PAGE_TRANSITION_LINK, true));
  ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 1, GURL(kTestUrl2),
                                     ui::PAGE_TRANSITION_LINK, true));
  base::RunLoop().RunUntilIdle();
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kMouse));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(controller_->entry_point_info_for_testing().has_value());
  ASSERT_EQ(3, browser()->tab_strip_model()->count());

  // Execute entry point and check a new tab is created with product
  // specification URL.
  controller_->OnEntryPointExecuted();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(4, browser()->tab_strip_model()->count());
  ASSERT_EQ(3, browser()->tab_strip_model()->active_index());
  content::WebContents* current_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(commerce::GetProductSpecsTabUrlForID(uuid),
            current_tab->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsEntryPointControllerBrowserTest,
                       ExecuteEntryPoint_IgnoreClosedTab) {
  // Set up product spec service.
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();
  commerce::ProductSpecificationsSet set(
      uuid.AsLowercaseString(), 0, 0, {GURL(kTestUrl1), GURL(kTestUrl2)}, "");
  ON_CALL(*mock_product_spec_service_, AddProductSpecificationsSet)
      .WillByDefault(testing::Return(set));

  // Mock EntryPointInfo returned by ShoppingService.
  std::map<GURL, uint64_t> similar_products = {{GURL(kTestUrl2), kProductId2},
                                               {GURL(kTestUrl3), kProductId3},
                                               {GURL(kTestUrl4), kProductId4}};
  auto info =
      std::make_optional<commerce::EntryPointInfo>(kTitle, similar_products);
  mock_cluster_manager_->SetResponseForGetEntryPointInfoForNavigation(info);

  // Mock that there is only two currently open unique URLs based on
  // ShoppingService.
  std::vector<commerce::UrlInfo> url_infos;
  commerce::UrlInfo info1;
  info1.url = GURL(kTestUrl2);
  url_infos.push_back(std::move(info1));
  commerce::UrlInfo info2;
  info2.url = GURL(kTestUrl3);
  url_infos.push_back(std::move(info2));
  commerce::UrlInfo info3;
  info3.url = GURL(kTestUrl3);
  url_infos.push_back(std::move(info3));
  ON_CALL(*mock_shopping_service_, GetUrlInfosForActiveWebWrappers)
      .WillByDefault(testing::Return(url_infos));

  // Only open URLs should be added to the set.
  std::vector<GURL> expected_urls = {GURL(kTestUrl3), GURL(kTestUrl2)};
  EXPECT_CALL(*mock_product_spec_service_,
              AddProductSpecificationsSet(kTitle, expected_urls))
      .Times(1);

  // Trigger entry point with navigations and execute the entry point.
  std::vector<std::string> urls_to_open = {kTestUrl2, kTestUrl3, kTestUrl4};
  for (auto& url : urls_to_open) {
    ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 0, GURL(url),
                                       ui::PAGE_TRANSITION_LINK, true));
    base::RunLoop().RunUntilIdle();
    controller_->OnClusterFinishedForNavigation(GURL(url));
    base::RunLoop().RunUntilIdle();
  }
  ASSERT_TRUE(controller_->entry_point_info_for_testing().has_value());
  controller_->OnEntryPointExecuted();
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsEntryPointControllerBrowserTest,
                       InvalidEntryPointWithNavigation) {
  // Mock EntryPointInfo returned by ClusterManager.
  std::map<GURL, uint64_t> similar_products = {{GURL(kTestUrl2), kProductId2},
                                               {GURL(kTestUrl3), kProductId3},
                                               {GURL(kTestUrl4), kProductId4}};
  auto info =
      std::make_optional<commerce::EntryPointInfo>(kTitle, similar_products);
  mock_cluster_manager_->SetResponseForGetEntryPointInfoForNavigation(info);

  // Set up observer.
  EXPECT_CALL(*observer_, ShowEntryPointWithTitle(kTitle)).Times(1);

  // Trigger entry point with navigations.
  std::vector<std::string> urls_to_open = {kTestUrl2, kTestUrl3, kTestUrl4};
  for (auto& url : urls_to_open) {
    ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 0, GURL(url),
                                       ui::PAGE_TRANSITION_LINK, true));
    base::RunLoop().RunUntilIdle();
    controller_->OnClusterFinishedForNavigation(GURL(url));
    base::RunLoop().RunUntilIdle();
  }
  ASSERT_TRUE(controller_->entry_point_info_for_testing().has_value());

  // Navigate to a URL that is not in cluster. After this navigation, there are
  // two URLs in this window that belong to the cluster, and the entry point is
  // still valid.
  auto* web_contents_one = browser()->tab_strip_model()->GetWebContentsAt(0);
  ASSERT_EQ(web_contents_one->GetLastCommittedURL(), GURL(kTestUrl4));
  ASSERT_TRUE(content::NavigateToURL(web_contents_one, GURL(kTestUrl1)));

  // Navigate to a URL that is not in cluster. After this navigation, there is
  // one URL in this window that belong to the cluster, and the entry point is
  // no longer valid.
  EXPECT_CALL(*observer_, HideEntryPoint()).Times(testing::AtLeast(1));
  auto* web_contents_two = browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_EQ(web_contents_two->GetLastCommittedURL(), GURL(kTestUrl3));
  ASSERT_TRUE(content::NavigateToURL(web_contents_two, GURL(kTestUrl1)));
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsEntryPointControllerBrowserTest,
                       InvalidEntryPointWithClosure) {
  // Mock EntryPointInfo returned by ShoppingService.
  std::map<GURL, uint64_t> similar_products = {{GURL(kTestUrl2), kProductId2},
                                               {GURL(kTestUrl3), kProductId3},
                                               {GURL(kTestUrl4), kProductId4}};
  auto info =
      std::make_optional<commerce::EntryPointInfo>(kTitle, similar_products);
  mock_cluster_manager_->SetResponseForGetEntryPointInfoForNavigation(info);

  // Set up observer.
  EXPECT_CALL(*observer_, ShowEntryPointWithTitle(kTitle)).Times(1);

  // Trigger entry point with navigations.
  std::vector<std::string> urls_to_open = {kTestUrl2, kTestUrl3, kTestUrl4};
  for (auto& url : urls_to_open) {
    ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 0, GURL(url),
                                       ui::PAGE_TRANSITION_LINK, true));
    base::RunLoop().RunUntilIdle();
    controller_->OnClusterFinishedForNavigation(GURL(url));
    base::RunLoop().RunUntilIdle();
  }
  ASSERT_TRUE(controller_->entry_point_info_for_testing().has_value());

  // Close a tab with URL that is in the cluster. After this closure, there are
  // two URLs in this window that belong to the cluster, and the entry point is
  // still valid.
  browser()->tab_strip_model()->CloseWebContentsAt(/*index=*/0,
                                                   TabCloseTypes::CLOSE_NONE);

  // Close a tab with URL that is in the cluster. After this closure, there is
  // one URL in this window that belong to the cluster, and the entry point is
  // no longer valid.
  EXPECT_CALL(*observer_, HideEntryPoint()).Times(testing::AtLeast(1));
  browser()->tab_strip_model()->CloseWebContentsAt(/*index=*/0,
                                                   TabCloseTypes::CLOSE_NONE);
}
