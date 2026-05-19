// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_promotion/search_promotion_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/ui/search_promotion/search_promotion_manager_factory.h"
#include "chrome/browser/ui/search_promotion/search_promotion_navigation_observer.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/segment_selection_result.h"
#include "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "url/gurl.h"

namespace {

class MockSearchPromotionManager : public SearchPromotionManager {
 public:
  explicit MockSearchPromotionManager(Profile& profile)
      : SearchPromotionManager(profile) {}
  MOCK_METHOD(void, OnTargetURLVisited, (const GURL& url), (override));
};

std::unique_ptr<KeyedService> BuildMockSearchPromotionManager(
    content::BrowserContext* context) {
  return std::make_unique<MockSearchPromotionManager>(
      *Profile::FromBrowserContext(context));
}

std::unique_ptr<KeyedService> BuildMockSegmentationPlatformService(
    content::BrowserContext* context) {
  return std::make_unique<testing::NiceMock<
      segmentation_platform::MockSegmentationPlatformService>>();
}

}  // namespace

class SearchPromotionManagerTest : public ChromeRenderViewHostTestHarness {
 protected:
  SearchPromotionManagerTest() = default;
  ~SearchPromotionManagerTest() override = default;

  bool IsPromoAllowed(SearchPromotionManager& manager) {
    return manager.IsPromoAllowedForTesting();
  }

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SearchPromotionManagerTest, IsPromoAllowedGuardedByFeature) {
  feature_list_.InitAndDisableFeature(
      feature_engagement::kIPHSearchPromotionFeature);
  {
    SearchPromotionManager manager(*profile());
    EXPECT_FALSE(IsPromoAllowed(manager));
  }

  feature_list_.Reset();
  feature_list_.InitAndEnableFeature(
      feature_engagement::kIPHSearchPromotionFeature);
  {
    SearchPromotionManager manager(*profile());
    EXPECT_TRUE(IsPromoAllowed(manager));
  }
}

TEST_F(SearchPromotionManagerTest, EngagementThresholdGating) {
  feature_list_.InitAndEnableFeature(
      feature_engagement::kIPHSearchPromotionFeature);

  segmentation_platform::MockSegmentationPlatformService* mock_service =
      static_cast<segmentation_platform::MockSegmentationPlatformService*>(
          segmentation_platform::SegmentationPlatformServiceFactory::
              GetInstance()
                  ->SetTestingFactoryAndUse(
                      profile(), base::BindRepeating(
                                     &BuildMockSegmentationPlatformService)));

  SearchPromotionManager* manager =
      SearchPromotionManagerFactory::GetForProfile(profile());
  ASSERT_TRUE(manager);

  // Case 1: Backend is not ready in cache, returns false (skips promo).
  segmentation_platform::SegmentSelectionResult not_ready_result;
  not_ready_result.is_ready = false;
  EXPECT_CALL(
      *mock_service,
      GetCachedSegmentResult(
          segmentation_platform::kChromeLowUserEngagementSegmentationKey))
      .WillOnce(testing::Return(not_ready_result));

  EXPECT_FALSE(manager->IsEngagementLowEnoughForTesting());

  segmentation_platform::SegmentSelectionResult low_engagement_result;
  low_engagement_result.is_ready = true;
  low_engagement_result.segment = segmentation_platform::proto::SegmentId::
      OPTIMIZATION_TARGET_SEGMENTATION_CHROME_LOW_USER_ENGAGEMENT;

  // Case 2: Backend is ready in cache, but no segment selected (e.g.
  // insufficient data).
  segmentation_platform::SegmentSelectionResult no_segment_result;
  no_segment_result.is_ready = true;
  no_segment_result.segment = std::nullopt;
  EXPECT_CALL(
      *mock_service,
      GetCachedSegmentResult(
          segmentation_platform::kChromeLowUserEngagementSegmentationKey))
      .WillOnce(testing::Return(no_segment_result));

  EXPECT_FALSE(manager->IsEngagementLowEnoughForTesting());

  // Case 3: Backend is ready in cache, but user is in a different segment (high
  // engagement).
  segmentation_platform::SegmentSelectionResult high_engagement_result;
  high_engagement_result.is_ready = true;
  high_engagement_result.segment = segmentation_platform::proto::SegmentId::
      OPTIMIZATION_TARGET_SEGMENTATION_DUMMY;
  EXPECT_CALL(
      *mock_service,
      GetCachedSegmentResult(
          segmentation_platform::kChromeLowUserEngagementSegmentationKey))
      .WillOnce(testing::Return(high_engagement_result));

  EXPECT_FALSE(manager->IsEngagementLowEnoughForTesting());

  // Case 4: Backend is ready in cache, and user is in low engagement segment.
  EXPECT_CALL(
      *mock_service,
      GetCachedSegmentResult(
          segmentation_platform::kChromeLowUserEngagementSegmentationKey))
      .WillOnce(testing::Return(low_engagement_result));

  EXPECT_TRUE(manager->IsEngagementLowEnoughForTesting());
}

TEST_F(SearchPromotionManagerTest, ObserverCallsManagerOnGoogleSearch) {
  // Set up the mock manager.
  SearchPromotionManagerFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildMockSearchPromotionManager));

  MockSearchPromotionManager* mock_manager =
      static_cast<MockSearchPromotionManager*>(
          SearchPromotionManagerFactory::GetForProfile(profile()));

  // Attach the observer.
  tabs::MockTabInterface tab_interface;
  ui::UnownedUserDataHost user_data_host;
  ON_CALL(tab_interface, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(user_data_host));
  ON_CALL(tab_interface, GetContents())
      .WillByDefault(testing::Return(web_contents()));

  SearchPromotionNavigationObserver observer(tab_interface);

  EXPECT_CALL(*mock_manager,
              OnTargetURLVisited(GURL("http://www.google.com/search?q=test")));

  // Simulate navigation.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("http://www.google.com/search?q=test"));
}

TEST_F(SearchPromotionManagerTest, ObserverIgnoresNonGoogleSearch) {
  // Set up the mock manager.
  SearchPromotionManagerFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildMockSearchPromotionManager));

  MockSearchPromotionManager* mock_manager =
      static_cast<MockSearchPromotionManager*>(
          SearchPromotionManagerFactory::GetForProfile(profile()));

  // Attach the observer.
  tabs::MockTabInterface tab_interface;
  ui::UnownedUserDataHost user_data_host;
  ON_CALL(tab_interface, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(user_data_host));
  ON_CALL(tab_interface, GetContents())
      .WillByDefault(testing::Return(web_contents()));

  SearchPromotionNavigationObserver observer(tab_interface);

  EXPECT_CALL(*mock_manager, OnTargetURLVisited(testing::_)).Times(0);

  // Simulate navigation to non-Google URL.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("http://www.example.com/"));
}
