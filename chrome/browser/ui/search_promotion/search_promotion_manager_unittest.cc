// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_promotion/search_promotion_manager.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_reg_util_win.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/platform_experience/delegated_tasks/delegated_task_runner.h"
#include "chrome/browser/platform_experience/delegated_tasks/test_support/mock_delegated_task_runner.h"
#include "chrome/browser/platform_experience/delegated_tasks/test_support/mock_peh_launcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/search_promotion/register_search_promotion_task.h"
#include "chrome/browser/ui/search_promotion/search_promotion_manager_factory.h"
#include "chrome/browser/ui/search_promotion/search_promotion_navigation_observer.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/user_education/mock_browser_user_education_interface.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/segmentation_platform/embedder/default_model/chrome_user_engagement.h"
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

std::unique_ptr<platform_experience::DelegatedTaskRunner>
CreateMockTaskRunner() {
  return std::make_unique<
      testing::NiceMock<platform_experience::MockDelegatedTaskRunner>>();
}

SearchPromotionManager::CreateTaskRunnerCallback
GetCreateMockTaskRunnerCallback() {
  return base::BindRepeating([]() { return CreateMockTaskRunner(); });
}

std::unique_ptr<platform_experience::PehLauncher>
CreateEligibleMockPehLauncher() {
  auto launcher = std::make_unique<
      testing::NiceMock<platform_experience::MockPehLauncher>>();
  ON_CALL(*launcher, GetBinaryPath())
      .WillByDefault(testing::Return(
          base::FilePath(FILE_PATH_LITERAL("C:\\test\\peh.exe"))));
  ON_CALL(*launcher, IsBinaryVerified(testing::_))
      .WillByDefault(testing::Return(true));
  ON_CALL(*launcher, GetBinaryVersion(testing::_))
      .WillByDefault(testing::Return(base::Version("999.0.0.0")));
  return launcher;
}

using PehLauncherFactory = base::RepeatingCallback<
    std::unique_ptr<platform_experience::PehLauncher>()>;

PehLauncherFactory GetEligibleMockPehLauncherFactory() {
  return base::BindRepeating(&CreateEligibleMockPehLauncher);
}

std::unique_ptr<platform_experience::PehLauncher>
CreateIneligibleMockPehLauncher() {
  auto launcher = std::make_unique<
      testing::NiceMock<platform_experience::MockPehLauncher>>();
  ON_CALL(*launcher, GetBinaryPath())
      .WillByDefault(testing::Return(base::FilePath()));
  return launcher;
}

PehLauncherFactory GetIneligibleMockPehLauncherFactory() {
  return base::BindRepeating(&CreateIneligibleMockPehLauncher);
}

class MockSearchPromotionManager : public SearchPromotionManager {
 public:
  MockSearchPromotionManager(Profile& profile,
                             CreateTaskRunnerCallback callback,
                             CreatePehLauncherCallback peh_callback =
                                 base::BindOnce(&CreateEligibleMockPehLauncher))
      : SearchPromotionManager(profile,
                               std::move(callback),
                               std::move(peh_callback)) {}
  MOCK_METHOD(void,
              OnTargetURLVisited,
              (BrowserUserEducationInterface & user_education),
              (override));
};

std::unique_ptr<KeyedService> BuildMockSearchPromotionManager(
    content::BrowserContext* context) {
  return std::make_unique<MockSearchPromotionManager>(
      *Profile::FromBrowserContext(context), GetCreateMockTaskRunnerCallback());
}

std::unique_ptr<KeyedService> BuildSearchPromotionManager(
    PehLauncherFactory peh_factory,
    content::BrowserContext* context) {
  return std::make_unique<SearchPromotionManager>(
      *Profile::FromBrowserContext(context), GetCreateMockTaskRunnerCallback(),
      std::move(peh_factory));
}

std::unique_ptr<KeyedService> BuildMockSegmentationPlatformService(
    content::BrowserContext* context) {
  return std::make_unique<testing::NiceMock<
      segmentation_platform::MockSegmentationPlatformService>>();
}

std::unique_ptr<KeyedService> BuildMockTracker(
    content::BrowserContext* context) {
  return std::make_unique<
      testing::NiceMock<feature_engagement::test::MockTracker>>();
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
        {{"action", feature_engagement::kSearchPromotionActionOpen}});
  }

  SearchPromotionManager* RecreateSearchPromotionManager(
      PehLauncherFactory peh_factory = GetEligibleMockPehLauncherFactory()) {
    auto* manager = static_cast<SearchPromotionManager*>(
        SearchPromotionManagerFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildSearchPromotionManager,
                                           std::move(peh_factory))));
    if (base::FeatureList::IsEnabled(
            feature_engagement::kIPHSearchPromotionFeature) &&
        feature_engagement::kSearchPromotionAction.Get() !=
            feature_engagement::SearchPromotionAction::kDisabled) {
      EXPECT_TRUE(base::test::RunUntil(
          [&]() { return manager->IsPehEligibleForTesting().has_value(); }));
    }
    return manager;
  }

  std::unique_ptr<SearchPromotionManager> CreateSearchPromotionManager(
      SearchPromotionManager::CreatePehLauncherCallback peh_callback =
          base::BindOnce(&CreateEligibleMockPehLauncher)) {
    return std::make_unique<SearchPromotionManager>(
        *profile(), GetCreateMockTaskRunnerCallback(), std::move(peh_callback));
  }

  base::test::ScopedFeatureList feature_list_;
  registry_util::RegistryOverrideManager registry_override_manager_;
};

TEST_F(SearchPromotionManagerTest, IsPromoAllowedGuardedByFeature) {
  feature_list_.InitAndDisableFeature(
      feature_engagement::kIPHSearchPromotionFeature);
  {
    auto manager = CreateSearchPromotionManager();
    EXPECT_FALSE(IsPromoAllowed(*manager));
  }

  feature_list_.Reset();
  InitSearchPromotionFeature();
  {
    auto manager = CreateSearchPromotionManager();
    EXPECT_TRUE(IsPromoAllowed(*manager));
  }
}

TEST_F(SearchPromotionManagerTest, PehEligibilityGating_Ineligible) {
  InitSearchPromotionFeature();

  SearchPromotionManager* manager =
      RecreateSearchPromotionManager(GetIneligibleMockPehLauncherFactory());

  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface;
  ui::UnownedUserDataHost window_user_data_host;
  ON_CALL(mock_browser_window_interface, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(window_user_data_host));
  MockBrowserUserEducationInterface mock_user_education(
      &mock_browser_window_interface);

  // Promo should NOT be shown when PEH is ineligible.
  EXPECT_CALL(mock_user_education, MaybeShowFeaturePromo(testing::_)).Times(0);

  base::HistogramTester histogram_tester;
  manager->OnTargetURLVisited(mock_user_education);

  // Evaluated metric is still recorded for baseline.
  histogram_tester.ExpectTotalCount("Search.SearchPromotion.Evaluated", 1);
  // DefaultBrowserState should not be recorded since it returns before
  // checking.
  histogram_tester.ExpectTotalCount(
      "Search.SearchPromotion.DefaultBrowserState", 0);
}

TEST_F(SearchPromotionManagerTest, PehEligibilityGating_Eligible) {
  InitSearchPromotionFeature();

  SearchPromotionManager* manager = RecreateSearchPromotionManager();

  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface;
  ui::UnownedUserDataHost window_user_data_host;
  ON_CALL(mock_browser_window_interface, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(window_user_data_host));
  MockBrowserUserEducationInterface mock_user_education(
      &mock_browser_window_interface);

  // Promo should be shown when PEH is eligible.
  EXPECT_CALL(mock_user_education, MaybeShowFeaturePromo(testing::_))
      .WillOnce(testing::Return(true));

  manager->OnTargetURLVisited(mock_user_education);
}

TEST_F(SearchPromotionManagerTest,
       PehEligibility_BackgroundQuerySetsEligibleState) {
  InitSearchPromotionFeature();

  base::HistogramTester histogram_tester;
  auto manager = CreateSearchPromotionManager();
  // Before background query runs, state is in pending / nullopt state.
  EXPECT_FALSE(manager->IsPehEligibleForTesting().has_value());

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return manager->IsPehEligibleForTesting().has_value(); }));

  // After background query runs, state is resolved to true and histogram is
  // logged.
  EXPECT_EQ(manager->IsPehEligibleForTesting(), true);
  histogram_tester.ExpectUniqueSample("Search.SearchPromotion.PehEligible",
                                      SearchPromotionPehEligibility::kEligible,
                                      1);
}

TEST_F(SearchPromotionManagerTest,
       PehEligibility_BackgroundQuerySetsIneligibleWhenLauncherUnavailable) {
  InitSearchPromotionFeature();

  base::HistogramTester histogram_tester;
  auto manager = CreateSearchPromotionManager(
      base::BindOnce([]() -> std::unique_ptr<platform_experience::PehLauncher> {
        return nullptr;
      }));
  EXPECT_FALSE(manager->IsPehEligibleForTesting().has_value());

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return manager->IsPehEligibleForTesting().has_value(); }));

  EXPECT_EQ(manager->IsPehEligibleForTesting(), false);
  histogram_tester.ExpectUniqueSample(
      "Search.SearchPromotion.PehEligible",
      SearchPromotionPehEligibility::kLauncherUnavailable, 1);
}

TEST_F(SearchPromotionManagerTest,
       PehEligibility_BackgroundQuerySetsIneligibleWhenBinaryMissing) {
  InitSearchPromotionFeature();

  base::HistogramTester histogram_tester;
  auto manager = CreateSearchPromotionManager(
      base::BindOnce(&CreateIneligibleMockPehLauncher));
  // Before background query runs, state is in pending / nullopt state.
  EXPECT_FALSE(manager->IsPehEligibleForTesting().has_value());

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return manager->IsPehEligibleForTesting().has_value(); }));

  // After background query runs, state is resolved to false.
  EXPECT_EQ(manager->IsPehEligibleForTesting(), false);
  histogram_tester.ExpectUniqueSample(
      "Search.SearchPromotion.PehEligible",
      SearchPromotionPehEligibility::kBinaryNotFound, 1);
}

TEST_F(SearchPromotionManagerTest,
       PehEligibility_BackgroundQuerySetsIneligibleWhenBinaryUnverified) {
  InitSearchPromotionFeature();

  auto create_unverified_launcher =
      []() -> std::unique_ptr<platform_experience::PehLauncher> {
    auto launcher = std::make_unique<
        testing::NiceMock<platform_experience::MockPehLauncher>>();
    ON_CALL(*launcher, GetBinaryPath())
        .WillByDefault(testing::Return(
            base::FilePath(FILE_PATH_LITERAL("C:\\test\\peh.exe"))));
    ON_CALL(*launcher, IsBinaryVerified(testing::_))
        .WillByDefault(testing::Return(false));
    return launcher;
  };

  base::HistogramTester histogram_tester;
  auto manager =
      CreateSearchPromotionManager(base::BindOnce(create_unverified_launcher));
  EXPECT_FALSE(manager->IsPehEligibleForTesting().has_value());

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return manager->IsPehEligibleForTesting().has_value(); }));

  EXPECT_EQ(manager->IsPehEligibleForTesting(), false);
  histogram_tester.ExpectUniqueSample(
      "Search.SearchPromotion.PehEligible",
      SearchPromotionPehEligibility::kBinaryNotVerified, 1);
}

TEST_F(SearchPromotionManagerTest,
       PehEligibility_BackgroundQuerySetsIneligibleWhenBinaryVersionInvalid) {
  InitSearchPromotionFeature();

  auto create_invalid_version_launcher =
      []() -> std::unique_ptr<platform_experience::PehLauncher> {
    auto launcher = std::make_unique<
        testing::NiceMock<platform_experience::MockPehLauncher>>();
    ON_CALL(*launcher, GetBinaryPath())
        .WillByDefault(testing::Return(
            base::FilePath(FILE_PATH_LITERAL("C:\\test\\peh.exe"))));
    ON_CALL(*launcher, IsBinaryVerified(testing::_))
        .WillByDefault(testing::Return(true));
    ON_CALL(*launcher, GetBinaryVersion(testing::_))
        .WillByDefault(testing::Return(base::Version()));
    return launcher;
  };

  base::HistogramTester histogram_tester;
  auto manager = CreateSearchPromotionManager(
      base::BindOnce(create_invalid_version_launcher));
  EXPECT_FALSE(manager->IsPehEligibleForTesting().has_value());

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return manager->IsPehEligibleForTesting().has_value(); }));

  EXPECT_EQ(manager->IsPehEligibleForTesting(), false);
  histogram_tester.ExpectUniqueSample(
      "Search.SearchPromotion.PehEligible",
      SearchPromotionPehEligibility::kBinaryVersionInvalid, 1);
}

TEST_F(SearchPromotionManagerTest,
       PehEligibility_BackgroundQuerySetsIneligibleWhenVersionTooLow) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      feature_engagement::kIPHSearchPromotionFeature,
      {{"action", feature_engagement::kSearchPromotionActionOpen},
       {"min_peh_version", "2.0.0.0"}});

  auto create_low_version_launcher =
      []() -> std::unique_ptr<platform_experience::PehLauncher> {
    auto launcher = std::make_unique<
        testing::NiceMock<platform_experience::MockPehLauncher>>();
    ON_CALL(*launcher, GetBinaryPath())
        .WillByDefault(testing::Return(
            base::FilePath(FILE_PATH_LITERAL("C:\\test\\peh.exe"))));
    ON_CALL(*launcher, IsBinaryVerified(testing::_))
        .WillByDefault(testing::Return(true));
    ON_CALL(*launcher, GetBinaryVersion(testing::_))
        .WillByDefault(testing::Return(base::Version("1.0.0.0")));
    return launcher;
  };

  base::HistogramTester histogram_tester;
  auto manager =
      CreateSearchPromotionManager(base::BindOnce(create_low_version_launcher));
  EXPECT_FALSE(manager->IsPehEligibleForTesting().has_value());

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return manager->IsPehEligibleForTesting().has_value(); }));

  EXPECT_EQ(manager->IsPehEligibleForTesting(), false);
  histogram_tester.ExpectUniqueSample(
      "Search.SearchPromotion.PehEligible",
      SearchPromotionPehEligibility::kBinaryVersionTooLow, 1);
}

TEST_F(SearchPromotionManagerTest,
       PehEligibility_BackgroundQuerySetsIneligibleWhenMinVersionEmpty) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      feature_engagement::kIPHSearchPromotionFeature,
      {{"action", feature_engagement::kSearchPromotionActionOpen},
       {"min_peh_version", ""}});

  base::HistogramTester histogram_tester;
  auto manager = CreateSearchPromotionManager();
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return manager->IsPehEligibleForTesting().has_value(); }));

  EXPECT_EQ(manager->IsPehEligibleForTesting(), false);
  histogram_tester.ExpectUniqueSample(
      "Search.SearchPromotion.PehEligible",
      SearchPromotionPehEligibility::kMinVersionInvalid, 1);
}

TEST_F(SearchPromotionManagerTest,
       PehEligibility_BackgroundQuerySetsIneligibleWhenMinVersionInvalid) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      feature_engagement::kIPHSearchPromotionFeature,
      {{"action", feature_engagement::kSearchPromotionActionOpen},
       {"min_peh_version", "invalid.version.str"}});

  base::HistogramTester histogram_tester;
  auto manager = CreateSearchPromotionManager();
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return manager->IsPehEligibleForTesting().has_value(); }));

  EXPECT_EQ(manager->IsPehEligibleForTesting(), false);
  histogram_tester.ExpectUniqueSample(
      "Search.SearchPromotion.PehEligible",
      SearchPromotionPehEligibility::kMinVersionInvalid, 1);
}

TEST_F(SearchPromotionManagerTest,
       PehEligibility_PromoSuppressedWhileCheckInFlight) {
  InitSearchPromotionFeature();

  auto manager = CreateSearchPromotionManager();
  ASSERT_FALSE(manager->IsPehEligibleForTesting().has_value());

  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface;
  ui::UnownedUserDataHost window_user_data_host;
  ON_CALL(mock_browser_window_interface, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(window_user_data_host));
  MockBrowserUserEducationInterface mock_user_education(
      &mock_browser_window_interface);

  // Promo should NOT be shown while eligibility check is still pending.
  EXPECT_CALL(mock_user_education, MaybeShowFeaturePromo(testing::_)).Times(0);

  manager->OnTargetURLVisited(mock_user_education);
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
          segmentation_platform::ChromeUserEngagement::kChromeUserEngagementKey,
          testing::_, testing::_, testing::_))
      .WillOnce(base::test::RunOnceCallback<3>(not_ready_result));

  SearchPromotionManager* manager = RecreateSearchPromotionManager();
  EXPECT_TRUE(manager->GetEngagementLabelForTesting().empty());
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
          segmentation_platform::ChromeUserEngagement::kChromeUserEngagementKey,
          testing::_, testing::_, testing::_))
      .WillOnce(base::test::RunOnceCallback<3>(no_segment_result));

  SearchPromotionManager* manager = RecreateSearchPromotionManager();
  EXPECT_TRUE(manager->GetEngagementLabelForTesting().empty());
}

TEST_F(SearchPromotionManagerTest, EngagementThresholdGating_PowerEngagement) {
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
  high_engagement_result.ordered_labels.push_back(
      std::string(SearchPromotionManager::kEngagementLabelPower));

  EXPECT_CALL(
      *mock_service,
      GetClassificationResult(
          segmentation_platform::ChromeUserEngagement::kChromeUserEngagementKey,
          testing::_, testing::_, testing::_))
      .WillOnce(base::test::RunOnceCallback<3>(high_engagement_result));

  SearchPromotionManager* manager = RecreateSearchPromotionManager();
  EXPECT_EQ(manager->GetEngagementLabelForTesting(),
            SearchPromotionManager::kEngagementLabelPower);
}

TEST_F(SearchPromotionManagerTest, EngagementThresholdGating_OneDayEngagement) {
  InitSearchPromotionFeature();

  segmentation_platform::MockSegmentationPlatformService* mock_service =
      static_cast<segmentation_platform::MockSegmentationPlatformService*>(
          segmentation_platform::SegmentationPlatformServiceFactory::
              GetInstance()
                  ->SetTestingFactoryAndUse(
                      profile(), base::BindRepeating(
                                     &BuildMockSegmentationPlatformService)));

  segmentation_platform::ClassificationResult oneday_engagement_result(
      segmentation_platform::PredictionStatus::kSucceeded);
  oneday_engagement_result.ordered_labels.push_back(
      std::string(SearchPromotionManager::kEngagementLabelOneDay));

  EXPECT_CALL(
      *mock_service,
      GetClassificationResult(
          segmentation_platform::ChromeUserEngagement::kChromeUserEngagementKey,
          testing::_, testing::_, testing::_))
      .WillOnce(base::test::RunOnceCallback<3>(oneday_engagement_result));

  SearchPromotionManager* manager = RecreateSearchPromotionManager();
  EXPECT_EQ(manager->GetEngagementLabelForTesting(),
            SearchPromotionManager::kEngagementLabelOneDay);
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
      std::string(SearchPromotionManager::kEngagementLabelLow));

  EXPECT_CALL(
      *mock_service,
      GetClassificationResult(
          segmentation_platform::ChromeUserEngagement::kChromeUserEngagementKey,
          testing::_, testing::_, testing::_))
      .WillOnce(base::test::RunOnceCallback<3>(low_engagement_result));

  SearchPromotionManager* manager = RecreateSearchPromotionManager();
  EXPECT_EQ(manager->GetEngagementLabelForTesting(),
            SearchPromotionManager::kEngagementLabelLow);
}

TEST_F(SearchPromotionManagerTest, EngagementThresholdGating_MediumEngagement) {
  InitSearchPromotionFeature();

  segmentation_platform::MockSegmentationPlatformService* mock_service =
      static_cast<segmentation_platform::MockSegmentationPlatformService*>(
          segmentation_platform::SegmentationPlatformServiceFactory::
              GetInstance()
                  ->SetTestingFactoryAndUse(
                      profile(), base::BindRepeating(
                                     &BuildMockSegmentationPlatformService)));

  segmentation_platform::ClassificationResult medium_result(
      segmentation_platform::PredictionStatus::kSucceeded);
  medium_result.ordered_labels.push_back(
      std::string(SearchPromotionManager::kEngagementLabelMedium));

  EXPECT_CALL(
      *mock_service,
      GetClassificationResult(
          segmentation_platform::ChromeUserEngagement::kChromeUserEngagementKey,
          testing::_, testing::_, testing::_))
      .WillOnce(base::test::RunOnceCallback<3>(medium_result));

  SearchPromotionManager* manager = RecreateSearchPromotionManager();
  EXPECT_EQ(manager->GetEngagementLabelForTesting(),
            SearchPromotionManager::kEngagementLabelMedium);
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
      std::string(SearchPromotionManager::kEngagementLabelLow));

  EXPECT_CALL(
      *mock_service,
      GetClassificationResult(
          segmentation_platform::ChromeUserEngagement::kChromeUserEngagementKey,
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
       PromoInteractionRecordsDefaultBrowserType_Accepted_Open) {
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
      std::string(SearchPromotionManager::kEngagementLabelLow));

  EXPECT_CALL(
      *mock_service,
      GetClassificationResult(
          segmentation_platform::ChromeUserEngagement::kChromeUserEngagementKey,
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
        "Search.SearchPromotion.DefaultBrowserType.Accepted.Open");
    return samples && samples->TotalCount() >= 1;
  }));

  // Verify that the action-specific categorized DefaultBrowserType histogram
  // was recorded and specifically contains DefaultBrowserType::kFirefox (which
  // is 4, since we mocked "Firefox" in SetUp).
  histogram_tester.ExpectBucketCount(
      "Search.SearchPromotion.DefaultBrowserType.Accepted.Open", 4, 1);
}

TEST_F(SearchPromotionManagerTest,
       PromoInteractionRecordsDefaultBrowserType_Accepted_Install) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      feature_engagement::kIPHSearchPromotionFeature,
      {{"action", feature_engagement::kSearchPromotionActionInstall}});

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
      std::string(SearchPromotionManager::kEngagementLabelLow));

  EXPECT_CALL(
      *mock_service,
      GetClassificationResult(
          segmentation_platform::ChromeUserEngagement::kChromeUserEngagementKey,
          testing::_, testing::_, testing::_))
      .WillOnce(base::test::RunOnceCallback<3>(low_engagement_result));

  SearchPromotionManager* manager = RecreateSearchPromotionManager();

  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface;
  ui::UnownedUserDataHost window_user_data_host;
  ON_CALL(mock_browser_window_interface, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(window_user_data_host));
  MockBrowserUserEducationInterface mock_user_education(
      &mock_browser_window_interface);

  base::OnceClosure close_callback;
  EXPECT_CALL(mock_user_education, MaybeShowFeaturePromo(testing::_))
      .WillOnce([&close_callback](user_education::FeaturePromoParams params) {
        close_callback = std::move(params.close_callback);
        return true;
      });

  manager->OnTargetURLVisited(mock_user_education);
  manager->OnPromoAccepted();

  ASSERT_TRUE(close_callback);
  base::HistogramTester histogram_tester;
  std::move(close_callback).Run();

  EXPECT_TRUE(base::test::RunUntil([&]() {
    auto samples = histogram_tester.GetHistogramSamplesSinceCreation(
        "Search.SearchPromotion.DefaultBrowserType.Accepted.Install");
    return samples && samples->TotalCount() >= 1;
  }));

  histogram_tester.ExpectBucketCount(
      "Search.SearchPromotion.DefaultBrowserType.Accepted.Install", 4, 1);
}

TEST_F(SearchPromotionManagerTest,
       PromoInteractionRecordsDefaultBrowserType_Dismissed_Open) {
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
      std::string(SearchPromotionManager::kEngagementLabelLow));

  EXPECT_CALL(
      *mock_service,
      GetClassificationResult(
          segmentation_platform::ChromeUserEngagement::kChromeUserEngagementKey,
          testing::_, testing::_, testing::_))
      .WillOnce(base::test::RunOnceCallback<3>(low_engagement_result));

  SearchPromotionManager* manager = RecreateSearchPromotionManager();

  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface;
  ui::UnownedUserDataHost window_user_data_host;
  ON_CALL(mock_browser_window_interface, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(window_user_data_host));
  MockBrowserUserEducationInterface mock_user_education(
      &mock_browser_window_interface);

  base::OnceClosure close_callback;
  EXPECT_CALL(mock_user_education, MaybeShowFeaturePromo(testing::_))
      .WillOnce([&close_callback](user_education::FeaturePromoParams params) {
        close_callback = std::move(params.close_callback);
        return true;
      });

  manager->OnTargetURLVisited(mock_user_education);

  // Close the promo directly without calling OnPromoAccepted().
  ASSERT_TRUE(close_callback);
  base::HistogramTester histogram_tester;
  std::move(close_callback).Run();

  EXPECT_TRUE(base::test::RunUntil([&]() {
    auto samples = histogram_tester.GetHistogramSamplesSinceCreation(
        "Search.SearchPromotion.DefaultBrowserType.Dismissed.Open");
    return samples && samples->TotalCount() >= 1;
  }));

  histogram_tester.ExpectBucketCount(
      "Search.SearchPromotion.DefaultBrowserType.Dismissed.Open", 4, 1);
}

TEST_F(SearchPromotionManagerTest,
       PromoInteractionRecordsDefaultBrowserType_Dismissed_Install) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      feature_engagement::kIPHSearchPromotionFeature,
      {{"action", feature_engagement::kSearchPromotionActionInstall}});

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
      std::string(SearchPromotionManager::kEngagementLabelLow));

  EXPECT_CALL(
      *mock_service,
      GetClassificationResult(
          segmentation_platform::ChromeUserEngagement::kChromeUserEngagementKey,
          testing::_, testing::_, testing::_))
      .WillOnce(base::test::RunOnceCallback<3>(low_engagement_result));

  SearchPromotionManager* manager = RecreateSearchPromotionManager();

  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface;
  ui::UnownedUserDataHost window_user_data_host;
  ON_CALL(mock_browser_window_interface, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(window_user_data_host));
  MockBrowserUserEducationInterface mock_user_education(
      &mock_browser_window_interface);

  base::OnceClosure close_callback;
  EXPECT_CALL(mock_user_education, MaybeShowFeaturePromo(testing::_))
      .WillOnce([&close_callback](user_education::FeaturePromoParams params) {
        close_callback = std::move(params.close_callback);
        return true;
      });

  manager->OnTargetURLVisited(mock_user_education);

  // Close the promo directly without calling OnPromoAccepted().
  ASSERT_TRUE(close_callback);
  base::HistogramTester histogram_tester;
  std::move(close_callback).Run();

  EXPECT_TRUE(base::test::RunUntil([&]() {
    auto samples = histogram_tester.GetHistogramSamplesSinceCreation(
        "Search.SearchPromotion.DefaultBrowserType.Dismissed.Install");
    return samples && samples->TotalCount() >= 1;
  }));

  histogram_tester.ExpectBucketCount(
      "Search.SearchPromotion.DefaultBrowserType.Dismissed.Install", 4, 1);
}

TEST_F(SearchPromotionManagerTest, CohortEngagementGatingMatrix) {
  // Test Medium cohort: eligible only on Medium engagement.
  {
    base::test::ScopedFeatureList medium_features;
    medium_features.InitAndEnableFeatureWithParameters(
        feature_engagement::kIPHSearchPromotionFeature,
        {{"action", feature_engagement::kSearchPromotionActionInstall},
         {"cohort", feature_engagement::kSearchPromotionCohortMedium}});

    segmentation_platform::MockSegmentationPlatformService* mock_service =
        static_cast<segmentation_platform::MockSegmentationPlatformService*>(
            segmentation_platform::SegmentationPlatformServiceFactory::
                GetInstance()
                    ->SetTestingFactoryAndUse(
                        profile(), base::BindRepeating(
                                       &BuildMockSegmentationPlatformService)));

    segmentation_platform::ClassificationResult medium_result(
        segmentation_platform::PredictionStatus::kSucceeded);
    medium_result.ordered_labels.push_back(
        std::string(SearchPromotionManager::kEngagementLabelMedium));

    EXPECT_CALL(*mock_service, GetClassificationResult(
                                   segmentation_platform::ChromeUserEngagement::
                                       kChromeUserEngagementKey,
                                   testing::_, testing::_, testing::_))
        .WillOnce(base::test::RunOnceCallback<3>(medium_result));

    SearchPromotionManager* manager = RecreateSearchPromotionManager();

    testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface;
    ui::UnownedUserDataHost window_user_data_host;
    ON_CALL(mock_browser_window_interface, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(window_user_data_host));
    MockBrowserUserEducationInterface mock_user_education(
        &mock_browser_window_interface);

    EXPECT_CALL(mock_user_education, MaybeShowFeaturePromo(testing::_))
        .WillOnce(testing::Return(true));

    manager->OnTargetURLVisited(mock_user_education);
  }

  // Test Power cohort: eligible only on Power engagement.
  {
    base::test::ScopedFeatureList power_features;
    power_features.InitAndEnableFeatureWithParameters(
        feature_engagement::kIPHSearchPromotionFeature,
        {{"action", feature_engagement::kSearchPromotionActionInstall},
         {"cohort", feature_engagement::kSearchPromotionCohortPower}});

    segmentation_platform::MockSegmentationPlatformService* mock_service =
        static_cast<segmentation_platform::MockSegmentationPlatformService*>(
            segmentation_platform::SegmentationPlatformServiceFactory::
                GetInstance()
                    ->SetTestingFactoryAndUse(
                        profile(), base::BindRepeating(
                                       &BuildMockSegmentationPlatformService)));

    segmentation_platform::ClassificationResult power_result(
        segmentation_platform::PredictionStatus::kSucceeded);
    power_result.ordered_labels.push_back(
        std::string(SearchPromotionManager::kEngagementLabelPower));

    EXPECT_CALL(*mock_service, GetClassificationResult(
                                   segmentation_platform::ChromeUserEngagement::
                                       kChromeUserEngagementKey,
                                   testing::_, testing::_, testing::_))
        .WillOnce(base::test::RunOnceCallback<3>(power_result));

    SearchPromotionManager* manager = RecreateSearchPromotionManager();

    testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface;
    ui::UnownedUserDataHost window_user_data_host;
    ON_CALL(mock_browser_window_interface, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(window_user_data_host));
    MockBrowserUserEducationInterface mock_user_education(
        &mock_browser_window_interface);

    EXPECT_CALL(mock_user_education, MaybeShowFeaturePromo(testing::_))
        .WillOnce(testing::Return(true));

    manager->OnTargetURLVisited(mock_user_education);
  }

  // Test Low cohort: eligible on OneDay engagement.
  {
    base::test::ScopedFeatureList low_features;
    low_features.InitAndEnableFeatureWithParameters(
        feature_engagement::kIPHSearchPromotionFeature,
        {{"action", feature_engagement::kSearchPromotionActionInstall},
         {"cohort", feature_engagement::kSearchPromotionCohortLow}});

    segmentation_platform::MockSegmentationPlatformService* mock_service =
        static_cast<segmentation_platform::MockSegmentationPlatformService*>(
            segmentation_platform::SegmentationPlatformServiceFactory::
                GetInstance()
                    ->SetTestingFactoryAndUse(
                        profile(), base::BindRepeating(
                                       &BuildMockSegmentationPlatformService)));

    segmentation_platform::ClassificationResult oneday_result(
        segmentation_platform::PredictionStatus::kSucceeded);
    oneday_result.ordered_labels.push_back(
        std::string(SearchPromotionManager::kEngagementLabelOneDay));

    EXPECT_CALL(*mock_service, GetClassificationResult(
                                   segmentation_platform::ChromeUserEngagement::
                                       kChromeUserEngagementKey,
                                   testing::_, testing::_, testing::_))
        .WillOnce(base::test::RunOnceCallback<3>(oneday_result));

    SearchPromotionManager* manager = RecreateSearchPromotionManager();

    testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface;
    ui::UnownedUserDataHost window_user_data_host;
    ON_CALL(mock_browser_window_interface, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(window_user_data_host));
    MockBrowserUserEducationInterface mock_user_education(
        &mock_browser_window_interface);

    EXPECT_CALL(mock_user_education, MaybeShowFeaturePromo(testing::_))
        .WillOnce(testing::Return(true));

    manager->OnTargetURLVisited(mock_user_education);
  }

  // Test Medium cohort with Low engagement: promo is NOT shown.
  {
    base::test::ScopedFeatureList medium_features;
    medium_features.InitAndEnableFeatureWithParameters(
        feature_engagement::kIPHSearchPromotionFeature,
        {{"action", feature_engagement::kSearchPromotionActionInstall},
         {"cohort", feature_engagement::kSearchPromotionCohortMedium}});

    segmentation_platform::MockSegmentationPlatformService* mock_service =
        static_cast<segmentation_platform::MockSegmentationPlatformService*>(
            segmentation_platform::SegmentationPlatformServiceFactory::
                GetInstance()
                    ->SetTestingFactoryAndUse(
                        profile(), base::BindRepeating(
                                       &BuildMockSegmentationPlatformService)));

    segmentation_platform::ClassificationResult low_result(
        segmentation_platform::PredictionStatus::kSucceeded);
    low_result.ordered_labels.push_back(
        std::string(SearchPromotionManager::kEngagementLabelLow));

    EXPECT_CALL(*mock_service, GetClassificationResult(
                                   segmentation_platform::ChromeUserEngagement::
                                       kChromeUserEngagementKey,
                                   testing::_, testing::_, testing::_))
        .WillOnce(base::test::RunOnceCallback<3>(low_result));

    SearchPromotionManager* manager = RecreateSearchPromotionManager();

    testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface;
    ui::UnownedUserDataHost window_user_data_host;
    ON_CALL(mock_browser_window_interface, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(window_user_data_host));
    MockBrowserUserEducationInterface mock_user_education(
        &mock_browser_window_interface);

    EXPECT_CALL(mock_user_education, MaybeShowFeaturePromo(testing::_))
        .Times(0);

    manager->OnTargetURLVisited(mock_user_education);
  }

  // Test Low cohort: eligible on Low engagement.
  {
    base::test::ScopedFeatureList low_features;
    low_features.InitAndEnableFeatureWithParameters(
        feature_engagement::kIPHSearchPromotionFeature,
        {{"action", feature_engagement::kSearchPromotionActionInstall},
         {"cohort", feature_engagement::kSearchPromotionCohortLow}});

    segmentation_platform::MockSegmentationPlatformService* mock_service =
        static_cast<segmentation_platform::MockSegmentationPlatformService*>(
            segmentation_platform::SegmentationPlatformServiceFactory::
                GetInstance()
                    ->SetTestingFactoryAndUse(
                        profile(), base::BindRepeating(
                                       &BuildMockSegmentationPlatformService)));

    segmentation_platform::ClassificationResult low_result(
        segmentation_platform::PredictionStatus::kSucceeded);
    low_result.ordered_labels.push_back(
        std::string(SearchPromotionManager::kEngagementLabelLow));

    EXPECT_CALL(*mock_service, GetClassificationResult(
                                   segmentation_platform::ChromeUserEngagement::
                                       kChromeUserEngagementKey,
                                   testing::_, testing::_, testing::_))
        .WillOnce(base::test::RunOnceCallback<3>(low_result));

    SearchPromotionManager* manager = RecreateSearchPromotionManager();

    testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface;
    ui::UnownedUserDataHost window_user_data_host;
    ON_CALL(mock_browser_window_interface, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(window_user_data_host));
    MockBrowserUserEducationInterface mock_user_education(
        &mock_browser_window_interface);

    EXPECT_CALL(mock_user_education, MaybeShowFeaturePromo(testing::_))
        .WillOnce(testing::Return(true));

    manager->OnTargetURLVisited(mock_user_education);
  }

  // Test Low cohort with Power engagement: promo is NOT shown.
  {
    base::test::ScopedFeatureList low_features;
    low_features.InitAndEnableFeatureWithParameters(
        feature_engagement::kIPHSearchPromotionFeature,
        {{"action", feature_engagement::kSearchPromotionActionInstall},
         {"cohort", feature_engagement::kSearchPromotionCohortLow}});

    segmentation_platform::MockSegmentationPlatformService* mock_service =
        static_cast<segmentation_platform::MockSegmentationPlatformService*>(
            segmentation_platform::SegmentationPlatformServiceFactory::
                GetInstance()
                    ->SetTestingFactoryAndUse(
                        profile(), base::BindRepeating(
                                       &BuildMockSegmentationPlatformService)));

    segmentation_platform::ClassificationResult power_result(
        segmentation_platform::PredictionStatus::kSucceeded);
    power_result.ordered_labels.push_back(
        std::string(SearchPromotionManager::kEngagementLabelPower));

    EXPECT_CALL(*mock_service, GetClassificationResult(
                                   segmentation_platform::ChromeUserEngagement::
                                       kChromeUserEngagementKey,
                                   testing::_, testing::_, testing::_))
        .WillOnce(base::test::RunOnceCallback<3>(power_result));

    SearchPromotionManager* manager = RecreateSearchPromotionManager();

    testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface;
    ui::UnownedUserDataHost window_user_data_host;
    ON_CALL(mock_browser_window_interface, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(window_user_data_host));
    MockBrowserUserEducationInterface mock_user_education(
        &mock_browser_window_interface);

    EXPECT_CALL(mock_user_education, MaybeShowFeaturePromo(testing::_))
        .Times(0);

    manager->OnTargetURLVisited(mock_user_education);
  }
}

TEST_F(SearchPromotionManagerTest,
       OnPromoAcceptedNotifiesFeatureEngagementTracker) {
  InitSearchPromotionFeature();

  auto* mock_tracker = static_cast<feature_engagement::test::MockTracker*>(
      feature_engagement::TrackerFactory::GetInstance()
          ->SetTestingFactoryAndUse(profile(),
                                    base::BindRepeating(&BuildMockTracker)));

  EXPECT_CALL(*mock_tracker,
              NotifyEvent(feature_engagement::events::kSearchPromotionAccepted))
      .Times(1);

  SearchPromotionManager* manager = RecreateSearchPromotionManager();
  manager->OnPromoAccepted();
  // Subsequent calls should be ignored due to idempotency guard.
  manager->OnPromoAccepted();
}

TEST_F(SearchPromotionManagerTest, OnPromoAcceptedWithNullTrackerDoesNotCrash) {
  InitSearchPromotionFeature();

  // Explicitly ensure TrackerFactory returns nullptr for profile.
  feature_engagement::TrackerFactory::GetInstance()->SetTestingFactory(
      profile(), BrowserContextKeyedServiceFactory::TestingFactory());

  SearchPromotionManager* manager = RecreateSearchPromotionManager();
  manager->OnPromoAccepted();
}

TEST_F(SearchPromotionManagerTest,
       ControlAction_RecordsEvaluationAndState_DoesNotShowPromo) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      feature_engagement::kIPHSearchPromotionFeature,
      {{"action", feature_engagement::kSearchPromotionActionControl}});

  SearchPromotionManager* manager = RecreateSearchPromotionManager();

  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface;
  ui::UnownedUserDataHost window_user_data_host;
  ON_CALL(mock_browser_window_interface, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(window_user_data_host));
  MockBrowserUserEducationInterface mock_user_education(
      &mock_browser_window_interface);

  EXPECT_CALL(mock_user_education, MaybeShowFeaturePromo(testing::_)).Times(0);

  manager->OnTargetURLVisited(mock_user_education);

  histogram_tester.ExpectUniqueSample("Search.SearchPromotion.Evaluated", true,
                                      1);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    auto samples = histogram_tester.GetHistogramSamplesSinceCreation(
        "Search.SearchPromotion.DefaultBrowserState");
    return samples && samples->TotalCount() >= 1;
  }));
}

TEST_F(SearchPromotionManagerTest,
       ControlAction_WithCohortGating_RespectsEngagementEligibility) {
  // Test Medium cohort Control with Low engagement: state is NOT recorded.
  {
    base::test::ScopedFeatureList medium_control_features;
    medium_control_features.InitAndEnableFeatureWithParameters(
        feature_engagement::kIPHSearchPromotionFeature,
        {{"action", feature_engagement::kSearchPromotionActionControl},
         {"cohort", feature_engagement::kSearchPromotionCohortMedium}});

    segmentation_platform::MockSegmentationPlatformService* mock_service =
        static_cast<segmentation_platform::MockSegmentationPlatformService*>(
            segmentation_platform::SegmentationPlatformServiceFactory::
                GetInstance()
                    ->SetTestingFactoryAndUse(
                        profile(), base::BindRepeating(
                                       &BuildMockSegmentationPlatformService)));

    segmentation_platform::ClassificationResult low_result(
        segmentation_platform::PredictionStatus::kSucceeded);
    low_result.ordered_labels.push_back(
        std::string(SearchPromotionManager::kEngagementLabelLow));

    EXPECT_CALL(*mock_service, GetClassificationResult(
                                   segmentation_platform::ChromeUserEngagement::
                                       kChromeUserEngagementKey,
                                   testing::_, testing::_, testing::_))
        .WillOnce(base::test::RunOnceCallback<3>(low_result));

    SearchPromotionManager* manager = RecreateSearchPromotionManager();

    testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface;
    ui::UnownedUserDataHost window_user_data_host;
    ON_CALL(mock_browser_window_interface, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(window_user_data_host));
    MockBrowserUserEducationInterface mock_user_education(
        &mock_browser_window_interface);

    EXPECT_CALL(mock_user_education, MaybeShowFeaturePromo(testing::_))
        .Times(0);

    base::HistogramTester histogram_tester;
    manager->OnTargetURLVisited(mock_user_education);

    histogram_tester.ExpectUniqueSample("Search.SearchPromotion.Evaluated",
                                        true, 1);
    histogram_tester.ExpectTotalCount(
        "Search.SearchPromotion.DefaultBrowserState", 0);
  }

  // Test Medium cohort Control with Medium engagement: state IS recorded,
  // promo is NOT shown.
  {
    base::test::ScopedFeatureList medium_control_features;
    medium_control_features.InitAndEnableFeatureWithParameters(
        feature_engagement::kIPHSearchPromotionFeature,
        {{"action", feature_engagement::kSearchPromotionActionControl},
         {"cohort", feature_engagement::kSearchPromotionCohortMedium}});

    segmentation_platform::MockSegmentationPlatformService* mock_service =
        static_cast<segmentation_platform::MockSegmentationPlatformService*>(
            segmentation_platform::SegmentationPlatformServiceFactory::
                GetInstance()
                    ->SetTestingFactoryAndUse(
                        profile(), base::BindRepeating(
                                       &BuildMockSegmentationPlatformService)));

    segmentation_platform::ClassificationResult medium_result(
        segmentation_platform::PredictionStatus::kSucceeded);
    medium_result.ordered_labels.push_back(
        std::string(SearchPromotionManager::kEngagementLabelMedium));

    EXPECT_CALL(*mock_service, GetClassificationResult(
                                   segmentation_platform::ChromeUserEngagement::
                                       kChromeUserEngagementKey,
                                   testing::_, testing::_, testing::_))
        .WillOnce(base::test::RunOnceCallback<3>(medium_result));

    SearchPromotionManager* manager = RecreateSearchPromotionManager();

    testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface;
    ui::UnownedUserDataHost window_user_data_host;
    ON_CALL(mock_browser_window_interface, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(window_user_data_host));
    MockBrowserUserEducationInterface mock_user_education(
        &mock_browser_window_interface);

    EXPECT_CALL(mock_user_education, MaybeShowFeaturePromo(testing::_))
        .Times(0);

    base::HistogramTester histogram_tester;
    manager->OnTargetURLVisited(mock_user_education);

    histogram_tester.ExpectUniqueSample("Search.SearchPromotion.Evaluated",
                                        true, 1);
    EXPECT_TRUE(base::test::RunUntil([&]() {
      auto samples = histogram_tester.GetHistogramSamplesSinceCreation(
          "Search.SearchPromotion.DefaultBrowserState");
      return samples && samples->TotalCount() >= 1;
    }));
  }
}

TEST_F(SearchPromotionManagerTest, CohortGating_CohortAllAllowsPromo) {
  base::test::ScopedFeatureList cohort_all_features;
  cohort_all_features.InitAndEnableFeatureWithParameters(
      feature_engagement::kIPHSearchPromotionFeature,
      {{"action", feature_engagement::kSearchPromotionActionOpen},
       {"cohort", feature_engagement::kSearchPromotionCohortAll}});

  SearchPromotionManager* manager = RecreateSearchPromotionManager();

  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface;
  ui::UnownedUserDataHost window_user_data_host;
  ON_CALL(mock_browser_window_interface, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(window_user_data_host));
  MockBrowserUserEducationInterface mock_user_education(
      &mock_browser_window_interface);

  EXPECT_CALL(mock_user_education, MaybeShowFeaturePromo(testing::_))
      .WillOnce(testing::Return(true));

  manager->OnTargetURLVisited(mock_user_education);
}

// Verifies that specifying an invalid experiment action falls back to
// SearchPromotionAction::kDisabled and disables the promotion.
TEST_F(SearchPromotionManagerTest, InvalidActionDisablesPromo) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      feature_engagement::kIPHSearchPromotionFeature,
      {{"action", "invalid_action"}});

  SearchPromotionManager* manager = RecreateSearchPromotionManager();
  EXPECT_FALSE(IsPromoAllowed(*manager));

  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface;
  ui::UnownedUserDataHost window_user_data_host;
  ON_CALL(mock_browser_window_interface, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(window_user_data_host));
  MockBrowserUserEducationInterface mock_user_education(
      &mock_browser_window_interface);

  EXPECT_CALL(mock_user_education, MaybeShowFeaturePromo(testing::_)).Times(0);
  manager->OnTargetURLVisited(mock_user_education);
}

// Verifies that specifying an unrecognized cohort string falls back to the
// default SearchPromotionCohort::kAll (targeting all users).
TEST_F(SearchPromotionManagerTest, InvalidCohortDefaultsToAll) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      feature_engagement::kIPHSearchPromotionFeature,
      {{"action", feature_engagement::kSearchPromotionActionOpen},
       {"cohort", "unknown_cohort"}});

  SearchPromotionManager* manager = RecreateSearchPromotionManager();
  EXPECT_TRUE(IsPromoAllowed(*manager));

  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface;
  ui::UnownedUserDataHost window_user_data_host;
  ON_CALL(mock_browser_window_interface, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(window_user_data_host));
  MockBrowserUserEducationInterface mock_user_education(
      &mock_browser_window_interface);

  EXPECT_CALL(mock_user_education, MaybeShowFeaturePromo(testing::_))
      .WillOnce(testing::Return(true));
  manager->OnTargetURLVisited(mock_user_education);
}

class SearchPromotionManagerTaskRunnerTest : public SearchPromotionManagerTest {
 protected:
  void SetUp() override {
    mock_runner_ = std::make_unique<
        testing::NiceMock<platform_experience::MockDelegatedTaskRunner>>();

    SearchPromotionManagerTest::SetUp();
    SearchPromotionManagerFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&SearchPromotionManagerTaskRunnerTest::
                                           BuildMockSearchPromotionManager,
                                       base::Unretained(this)));
  }

  std::unique_ptr<KeyedService> BuildMockSearchPromotionManager(
      content::BrowserContext* context) {
    return std::make_unique<MockSearchPromotionManager>(
        *Profile::FromBrowserContext(context),
        base::BindRepeating(
            &SearchPromotionManagerTaskRunnerTest::CreateMockTaskRunner,
            base::Unretained(this)));
  }

  std::unique_ptr<platform_experience::DelegatedTaskRunner>
  CreateMockTaskRunner() {
    return std::move(mock_runner_);
  }

  SearchPromotionManager* manager() {
    return SearchPromotionManagerFactory::GetForProfile(profile());
  }

  // Holds ownership until CreateMockTaskRunner() moves it out.
  std::unique_ptr<platform_experience::MockDelegatedTaskRunner> mock_runner_;
};

TEST_F(SearchPromotionManagerTaskRunnerTest, PerformOpenActionSuccess) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      feature_engagement::kIPHSearchPromotionFeature,
      {{"action", feature_engagement::kSearchPromotionActionOpen},
       {"store_url", "https://google.com/store"}});

  base::test::TestFuture<void> future;
  EXPECT_CALL(*mock_runner_, Run(testing::_, testing::_, testing::_))
      .WillOnce(
          [&](std::unique_ptr<platform_experience::DelegatedTask> task,
              std::string_view min_version,
              platform_experience::DelegatedTaskCompletionCallback callback) {
            auto* promo_task =
                static_cast<RegisterSearchPromotionTask*>(task.get());
            EXPECT_EQ(promo_task->post_install_url(),
                      GURL("https://google.com/store"));
            EXPECT_TRUE(promo_task->extension_id().empty());
            std::move(callback).Run(
                {static_cast<int>(SearchPromotionExitCode::kUrlLaunchSuccess),
                 base::Milliseconds(100)});
            future.GetCallback().Run();
          });

  manager()->OnPromoAccepted();
  EXPECT_TRUE(future.Wait());
  histogram_tester.ExpectUniqueSample(
      "Search.SearchPromotion.DelegatedTaskExitCode",
      SearchPromotionExitCode::kUrlLaunchSuccess, 1);
  histogram_tester.ExpectUniqueTimeSample(
      "Search.SearchPromotion.Duration.UrlLaunchSuccess",
      base::Milliseconds(100), 1);
}

TEST_F(SearchPromotionManagerTaskRunnerTest, PerformInstallActionSuccess) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      feature_engagement::kIPHSearchPromotionFeature,
      {{"action", feature_engagement::kSearchPromotionActionInstall},
       {"extension_id", "test_extension_id"},
       {"instructions_url", "https://google.com/instructions"}});

  base::test::TestFuture<void> future;
  EXPECT_CALL(*mock_runner_, Run(testing::_, testing::_, testing::_))
      .WillOnce(
          [&](std::unique_ptr<platform_experience::DelegatedTask> task,
              std::string_view min_version,
              platform_experience::DelegatedTaskCompletionCallback callback) {
            auto* promo_task =
                static_cast<RegisterSearchPromotionTask*>(task.get());
            EXPECT_EQ(promo_task->post_install_url(),
                      GURL("https://google.com/instructions"));
            EXPECT_EQ(promo_task->extension_id(), "test_extension_id");
            std::move(callback).Run(
                {static_cast<int>(SearchPromotionExitCode::kSuccessBackground),
                 base::Milliseconds(200)});
            future.GetCallback().Run();
          });

  manager()->OnPromoAccepted();
  EXPECT_TRUE(future.Wait());
  histogram_tester.ExpectUniqueSample(
      "Search.SearchPromotion.DelegatedTaskExitCode",
      SearchPromotionExitCode::kSuccessBackground, 1);
  histogram_tester.ExpectUniqueTimeSample(
      "Search.SearchPromotion.Duration.SuccessBackground",
      base::Milliseconds(200), 1);
}

TEST_F(SearchPromotionManagerTaskRunnerTest,
       CustomMinPehVersionPassedToRunner) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      feature_engagement::kIPHSearchPromotionFeature,
      {{"action", feature_engagement::kSearchPromotionActionInstall},
       {"extension_id", "test_extension_id"},
       {"instructions_url", "https://google.com/instructions"},
       {"min_peh_version", "1.2.3.4"}});

  base::test::TestFuture<void> future;
  EXPECT_CALL(*mock_runner_, Run(testing::_, "1.2.3.4", testing::_))
      .WillOnce(
          [&](std::unique_ptr<platform_experience::DelegatedTask> task,
              std::string_view min_version,
              platform_experience::DelegatedTaskCompletionCallback callback) {
            std::move(callback).Run({});
            future.GetCallback().Run();
          });

  manager()->OnPromoAccepted();
  EXPECT_TRUE(future.Wait());
}

TEST_F(SearchPromotionManagerTaskRunnerTest, InvalidPostInstallUrl) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      feature_engagement::kIPHSearchPromotionFeature,
      {{"action", feature_engagement::kSearchPromotionActionOpen},
       {"store_url", "1234"}});

  EXPECT_CALL(*mock_runner_, Run(testing::_, testing::_, testing::_)).Times(0);

  manager()->OnPromoAccepted();
}

TEST_F(SearchPromotionManagerTaskRunnerTest, EmptyPostInstallUrl) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      feature_engagement::kIPHSearchPromotionFeature,
      {{"action", feature_engagement::kSearchPromotionActionOpen},
       {"store_url", ""}});

  EXPECT_CALL(*mock_runner_, Run(testing::_, testing::_, testing::_)).Times(0);

  manager()->OnPromoAccepted();
}

TEST_F(SearchPromotionManagerTaskRunnerTest, PromoFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      feature_engagement::kIPHSearchPromotionFeature);

  EXPECT_CALL(*mock_runner_, Run(testing::_, testing::_, testing::_)).Times(0);

  manager()->OnPromoAccepted();
}
