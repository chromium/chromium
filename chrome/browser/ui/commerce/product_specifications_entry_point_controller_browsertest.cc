// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commerce/product_specifications_entry_point_controller.h"

#include "base/uuid.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/commerce/core/commerce_utils.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/product_specifications/product_specifications_service.h"
#include "components/commerce/core/product_specifications/product_specifications_set.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kTitle[] = "test_tile";
const char kTestUrl1[] = "chrome://new-tab-page/";
const char kTestUrl2[] = "chrome://version/";
}  // namespace

class MockObserver
    : public commerce::ProductSpecificationsEntryPointController::Observer {
 public:
  MOCK_METHOD(void,
              ShowEntryPointWithTitle,
              (const std::string title),
              (override));
};

class MockProductSpecificationsService
    : public commerce::ProductSpecificationsService {
 public:
  MockProductSpecificationsService() : ProductSpecificationsService(nullptr) {}
  ~MockProductSpecificationsService() override = default;
  MOCK_METHOD(const std::optional<commerce::ProductSpecificationsSet>,
              AddProductSpecificationsSet,
              (const std::string& name, const std::vector<GURL>& urls),
              (override));
};

class ProductSpecificationsEntryPointControllerBrowserTest
    : public InProcessBrowserTest {
 public:
  ProductSpecificationsEntryPointControllerBrowserTest() = default;

  void SetUpOnMainThread() override {
    mock_shopping_service_ = static_cast<commerce::MockShoppingService*>(
        commerce::ShoppingServiceFactory::GetForBrowserContext(
            browser()->profile()));
    product_spec_service_ =
        std::make_unique<MockProductSpecificationsService>();
    ON_CALL(*mock_shopping_service_, GetProductSpecificationsService)
        .WillByDefault(testing::Return(product_spec_service_.get()));

    controller_ =
        std::make_unique<commerce::ProductSpecificationsEntryPointController>(
            browser());
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
  std::unique_ptr<MockProductSpecificationsService> product_spec_service_;
  std::unique_ptr<commerce::ProductSpecificationsEntryPointController>
      controller_;
  base::CallbackListSubscription create_services_subscription_;
  bool is_browser_context_services_created{false};

 private:
  base::WeakPtrFactory<ProductSpecificationsEntryPointControllerBrowserTest>
      weak_ptr_factory_{this};
};

IN_PROC_BROWSER_TEST_F(ProductSpecificationsEntryPointControllerBrowserTest,
                       TriggerEntryPointWithSelection) {
  // Mock EntryPointInfo returned by ShoppingService.
  auto info =
      std::make_optional<commerce::EntryPointInfo>(kTitle, std::set<GURL>());
  mock_shopping_service_->SetResponseForGetEntryPointInfoForSelection(info);

  // Set up observer.
  MockObserver observer;
  controller_->AddObserver(&observer);
  EXPECT_CALL(observer, ShowEntryPointWithTitle(kTitle)).Times(1);

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
                       HideEntryPoint) {
  // Trigger entry point with selection.
  auto info =
      std::make_optional<commerce::EntryPointInfo>(kTitle, std::set<GURL>());
  mock_shopping_service_->SetResponseForGetEntryPointInfoForSelection(info);
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
  EXPECT_CALL(*product_spec_service_,
              AddProductSpecificationsSet(testing::_, testing::_))
      .Times(1);
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();
  commerce::ProductSpecificationsSet set(
      uuid.AsLowercaseString(), 0, 0, {GURL(kTestUrl1), GURL(kTestUrl2)}, "");
  ON_CALL(*product_spec_service_, AddProductSpecificationsSet)
      .WillByDefault(testing::Return(set));

  // Trigger entry point with selection.
  std::set<GURL> urls = {GURL(kTestUrl1), GURL(kTestUrl2)};
  auto info = std::make_optional<commerce::EntryPointInfo>(kTitle, urls);
  mock_shopping_service_->SetResponseForGetEntryPointInfoForSelection(info);
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
