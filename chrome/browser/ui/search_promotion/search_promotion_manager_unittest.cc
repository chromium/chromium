// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_promotion/search_promotion_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/search_promotion/search_promotion_manager_factory.h"
#include "chrome/browser/ui/search_promotion/search_promotion_navigation_observer.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/user_education/mock_browser_user_education_interface.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/result.h"
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
  MOCK_METHOD(void,
              OnTargetURLVisited,
              (BrowserUserEducationInterface & user_education),
              (override));
};

std::unique_ptr<KeyedService> BuildMockSearchPromotionManager(
    content::BrowserContext* context) {
  return std::make_unique<MockSearchPromotionManager>(
      *Profile::FromBrowserContext(context));
}

std::unique_ptr<KeyedService> BuildSearchPromotionManager(
    content::BrowserContext* context) {
  return std::make_unique<SearchPromotionManager>(
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

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Redirect HKEY_CLASSES_ROOT registry queries to a temporary process-local
    // key. This isolates the test from the host OS and mocks the default
    // browser name returned by shell_integration::GetApplicationNameForScheme.
    //
    // This follows the standard registry virtualization pattern utilized in:
    // - chrome/browser/default_browser/default_browser_manager_unittest.cc
    // - chrome/browser/win/conflicts/enumerate_shell_extensions_unittest.cc
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CLASSES_ROOT));
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));

    base::win::RegKey key(HKEY_CLASSES_ROOT, L"https", KEY_WRITE);
    ASSERT_TRUE(key.Valid());

    // Mock friendly name of default browser as "Firefox" (read by
    // AssocQueryString).
    ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(nullptr, L"Firefox"));

    // Satisfy IsValidCustomScheme validation.
    ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(L"URL Protocol", L""));
  }

  bool IsPromoAllowed(const SearchPromotionManager& manager) {
    return manager.IsPromoAllowedForTesting();
  }

  void InitSearchPromotionFeature() {
    feature_list_.InitAndEnableFeatureWithParameters(
        feature_engagement::kIPHSearchPromotionFeature,
        {{"arm", feature_engagement::kSearchPromotionArmA}});
  }

  SearchPromotionManager* RecreateSearchPromotionManager() {
    return static_cast<SearchPromotionManager*>(
        SearchPromotionManagerFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildSearchPromotionManager)));
  }

  base::test::ScopedFeatureList feature_list_;
  registry_util::RegistryOverrideManager registry_override_manager_;
};

TEST_F(SearchPromotionManagerTest, IsPromoAllowedGuardedByFeature) {
  feature_list_.InitAndDisableFeature(
      feature_engagement::kIPHSearchPromotionFeature);
  {
    SearchPromotionManager manager(*profile());
    EXPECT_FALSE(IsPromoAllowed(manager));
  }

  feature_list_.Reset();
  InitSearchPromotionFeature();
  {
    SearchPromotionManager manager(*profile());
    EXPECT_TRUE(IsPromoAllowed(manager));
  }
}

TEST_F(SearchPromotionManagerTest, EngagementThresholdGating_NotReady) {
  InitSearchPromotionFeature();

  segmentation_platform::MockSegmentationPlatformService* mock_service =
      static_cast<segmentation_platform::MockSegmentationPlatformService*>(
          segmentation_platform::SegmentationPlatformServiceFactory::
              GetInstance()
                  ->SetTestingFactoryAndUse(
                      profile(), base::BindRepeating(
                                     &BuildMockSegmentationPlatformService)));

  segmentation_platform::ClassificationResult not_ready_result(
      segmentation_platform::PredictionStatus::kNotReady);

  EXPECT_CALL(
      *mock_service,
      GetClassificationResult(
          segmentation_platform::kChromeLowUserEngagementSegmentationKey,
          testing::_, testing::_, testing::_))
      .WillOnce(base::test::RunOnceCallback<3>(not_ready_result));

  SearchPromotionManager* manager = RecreateSearchPromotionManager();
  EXPECT_FALSE(manager->IsEngagementLowEnoughForTesting());
}

TEST_F(SearchPromotionManagerTest, EngagementThresholdGating_NoSegment) {
  InitSearchPromotionFeature();

  segmentation_platform::MockSegmentationPlatformService* mock_service =
      static_cast<segmentation_platform::MockSegmentationPlatformService*>(
          segmentation_platform::SegmentationPlatformServiceFactory::
              GetInstance()
                  ->SetTestingFactoryAndUse(
                      profile(), base::BindRepeating(
                                     &BuildMockSegmentationPlatformService)));

  segmentation_platform::ClassificationResult no_segment_result(
      segmentation_platform::PredictionStatus::kSucceeded);

  EXPECT_CALL(
      *mock_service,
      GetClassificationResult(
          segmentation_platform::kChromeLowUserEngagementSegmentationKey,
          testing::_, testing::_, testing::_))
      .WillOnce(base::test::RunOnceCallback<3>(no_segment_result));

  SearchPromotionManager* manager = RecreateSearchPromotionManager();
  EXPECT_FALSE(manager->IsEngagementLowEnoughForTesting());
}

TEST_F(SearchPromotionManagerTest, EngagementThresholdGating_HighEngagement) {
  InitSearchPromotionFeature();

  segmentation_platform::MockSegmentationPlatformService* mock_service =
      static_cast<segmentation_platform::MockSegmentationPlatformService*>(
          segmentation_platform::SegmentationPlatformServiceFactory::
              GetInstance()
                  ->SetTestingFactoryAndUse(
                      profile(), base::BindRepeating(
                                     &BuildMockSegmentationPlatformService)));

  segmentation_platform::ClassificationResult high_engagement_result(
      segmentation_platform::PredictionStatus::kSucceeded);
  high_engagement_result.ordered_labels.push_back("LegacyNegativeLabel");

  EXPECT_CALL(
      *mock_service,
      GetClassificationResult(
          segmentation_platform::kChromeLowUserEngagementSegmentationKey,
          testing::_, testing::_, testing::_))
      .WillOnce(base::test::RunOnceCallback<3>(high_engagement_result));

  SearchPromotionManager* manager = RecreateSearchPromotionManager();
  EXPECT_FALSE(manager->IsEngagementLowEnoughForTesting());
}

TEST_F(SearchPromotionManagerTest, EngagementThresholdGating_LowEngagement) {
  InitSearchPromotionFeature();

  segmentation_platform::MockSegmentationPlatformService* mock_service =
      static_cast<segmentation_platform::MockSegmentationPlatformService*>(
          segmentation_platform::SegmentationPlatformServiceFactory::
              GetInstance()
                  ->SetTestingFactoryAndUse(
                      profile(), base::BindRepeating(
                                     &BuildMockSegmentationPlatformService)));

  segmentation_platform::ClassificationResult low_engagement_result(
      segmentation_platform::PredictionStatus::kSucceeded);
  low_engagement_result.ordered_labels.push_back(
      segmentation_platform::kChromeLowUserEngagementUmaName);

  EXPECT_CALL(
      *mock_service,
      GetClassificationResult(
          segmentation_platform::kChromeLowUserEngagementSegmentationKey,
          testing::_, testing::_, testing::_))
      .WillOnce(base::test::RunOnceCallback<3>(low_engagement_result));

  SearchPromotionManager* manager = RecreateSearchPromotionManager();
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
  ui::UnownedUserDataHost tab_user_data_host;
  ON_CALL(tab_interface, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(tab_user_data_host));
  ON_CALL(tab_interface, GetContents())
      .WillByDefault(testing::Return(web_contents()));

  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface;
  ui::UnownedUserDataHost window_user_data_host;
  ON_CALL(mock_browser_window_interface, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(window_user_data_host));

  MockBrowserUserEducationInterface mock_user_education(
      &mock_browser_window_interface);

  ON_CALL(tab_interface, GetBrowserWindowInterface())
      .WillByDefault(testing::Return(&mock_browser_window_interface));

  SearchPromotionNavigationObserver observer(tab_interface);

  // Verify that navigating to a Google Search URL successfully triggers
  // OnTargetURLVisited with the mock user education interface.
  EXPECT_CALL(*mock_manager,
              OnTargetURLVisited(testing::Ref(mock_user_education)));

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

TEST_F(SearchPromotionManagerTest, GatingEligibleRecordsDefaultBrowserState) {
  InitSearchPromotionFeature();

  segmentation_platform::MockSegmentationPlatformService* mock_service =
      static_cast<segmentation_platform::MockSegmentationPlatformService*>(
          segmentation_platform::SegmentationPlatformServiceFactory::
              GetInstance()
                  ->SetTestingFactoryAndUse(
                      profile(), base::BindRepeating(
                                     &BuildMockSegmentationPlatformService)));

  segmentation_platform::ClassificationResult low_engagement_result(
      segmentation_platform::PredictionStatus::kSucceeded);
  low_engagement_result.ordered_labels.push_back(
      segmentation_platform::kChromeLowUserEngagementUmaName);

  EXPECT_CALL(
      *mock_service,
      GetClassificationResult(
          segmentation_platform::kChromeLowUserEngagementSegmentationKey,
          testing::_, testing::_, testing::_))
      .WillOnce(base::test::RunOnceCallback<3>(low_engagement_result));

  SearchPromotionManager* manager = RecreateSearchPromotionManager();

  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface;
  // Instantiating a dummy UnownedUserDataHost for the
  // MockBrowserWindowInterface. MockBrowserUserEducationInterface expects the
  // window mock to return a valid reference from GetUnownedUserDataHost();
  // providing this prevents null-pointer dereferences or test harness crashes.
  ui::UnownedUserDataHost window_user_data_host;
  ON_CALL(mock_browser_window_interface, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(window_user_data_host));
  MockBrowserUserEducationInterface mock_user_education(
      &mock_browser_window_interface);

  EXPECT_CALL(mock_user_education, MaybeShowFeaturePromo(testing::_))
      .WillOnce(testing::Return(true));

  base::HistogramTester histogram_tester;
  manager->OnTargetURLVisited(mock_user_education);

  // Let the background thread pool task run and finish.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    auto samples = histogram_tester.GetHistogramSamplesSinceCreation(
        "Search.SearchPromotion.DefaultBrowserState");
    return samples && samples->TotalCount() >= 1;
  }));

  // Verify that the DefaultBrowserState histogram was successfully recorded
  // exactly once.
  histogram_tester.ExpectTotalCount(
      "Search.SearchPromotion.DefaultBrowserState", 1);
}

TEST_F(SearchPromotionManagerTest,
       PromoInteractionRecordsDefaultBrowserType_Accepted_ArmA) {
  InitSearchPromotionFeature();

  segmentation_platform::MockSegmentationPlatformService* mock_service =
      static_cast<segmentation_platform::MockSegmentationPlatformService*>(
          segmentation_platform::SegmentationPlatformServiceFactory::
              GetInstance()
                  ->SetTestingFactoryAndUse(
                      profile(), base::BindRepeating(
                                     &BuildMockSegmentationPlatformService)));

  segmentation_platform::ClassificationResult low_engagement_result(
      segmentation_platform::PredictionStatus::kSucceeded);
  low_engagement_result.ordered_labels.push_back(
      segmentation_platform::kChromeLowUserEngagementUmaName);

  EXPECT_CALL(
      *mock_service,
      GetClassificationResult(
          segmentation_platform::kChromeLowUserEngagementSegmentationKey,
          testing::_, testing::_, testing::_))
      .WillOnce(base::test::RunOnceCallback<3>(low_engagement_result));

  SearchPromotionManager* manager = RecreateSearchPromotionManager();

  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface;
  // Instantiating a dummy UnownedUserDataHost for the
  // MockBrowserWindowInterface. MockBrowserUserEducationInterface expects the
  // window mock to return a valid reference from GetUnownedUserDataHost();
  // providing this prevents null-pointer dereferences or test harness crashes.
  ui::UnownedUserDataHost window_user_data_host;
  ON_CALL(mock_browser_window_interface, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(window_user_data_host));
  MockBrowserUserEducationInterface mock_user_education(
      &mock_browser_window_interface);

  // Capture the close callback from the FeaturePromoParams passed to
  // MaybeShowFeaturePromo.
  base::OnceClosure close_callback;
  EXPECT_CALL(mock_user_education, MaybeShowFeaturePromo(testing::_))
      .WillOnce([&close_callback](user_education::FeaturePromoParams params) {
        close_callback = std::move(params.close_callback);
        return true;
      });

  manager->OnTargetURLVisited(mock_user_education);

  // Since dynamic action callbacks are not supported on FeaturePromoParams in
  // this version of User Education, the Custom Action is registered statically
  // at startup in browser_user_education_service.cc (which calls
  // manager->OnPromoAccepted() directly).
  //
  // We simulate this flow by invoking OnPromoAccepted() directly on the
  // manager, then executing the close callback to trigger metrics logging.
  manager->OnPromoAccepted();

  // Now close the promo, triggering close_callback.
  ASSERT_TRUE(close_callback);
  base::HistogramTester histogram_tester;
  std::move(close_callback).Run();

  // Let the background thread pool task run and finish.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    auto samples = histogram_tester.GetHistogramSamplesSinceCreation(
        "Search.SearchPromotion.DefaultBrowserType.Accepted.ArmA");
    return samples && samples->TotalCount() >= 1;
  }));

  // Verify that the arm-specific categorized DefaultBrowserType histogram was
  // recorded and specifically contains DefaultBrowserType::kFirefox (which is
  // 4, since we mocked "Firefox" in SetUp).
  histogram_tester.ExpectBucketCount(
      "Search.SearchPromotion.DefaultBrowserType.Accepted.ArmA", 4, 1);
}
