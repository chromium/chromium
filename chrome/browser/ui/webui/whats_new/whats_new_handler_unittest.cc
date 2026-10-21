// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_handler.h"

#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/whats_new/whats_new.mojom.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_ui.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/user_education/webui/mock_whats_new_storage_service.h"
#include "components/user_education/webui/whats_new_registry.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using whats_new::WhatsNewRegistry;

namespace {

// Modules
BASE_FEATURE(kTestEdition, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

class WhatsNewHandlerTest : public testing::Test {
 public:
  WhatsNewHandlerTest()
      : profile_(std::make_unique<TestingProfile>()),
        web_contents_(factory_.CreateWebContents(profile_.get())) {
  }
  ~WhatsNewHandlerTest() override = default;

  void SetUp() override {
    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile_.get(), base::BindRepeating(&BuildMockHatsService)));
    EXPECT_CALL(*mock_hats_service(), CanShowAnySurvey)
        .WillRepeatedly(testing::Return(true));

    // Setup mock storage service for tests that use the registry.
    auto mock_storage_service =
        std::make_unique<testing::NiceMock<MockWhatsNewStorageService>>();
    mock_storage_service_ = mock_storage_service.get();
    EXPECT_CALL(*mock_storage_service, ReadModuleData)
        .WillRepeatedly(testing::ReturnRef(mock_module_data_));

    whats_new_registry_ =
        std::make_unique<WhatsNewRegistry>(std::move(mock_storage_service));

    handler_ = std::make_unique<WhatsNewHandler>(
        mojo::PendingReceiver<whats_new::mojom::PageHandler>(), profile_.get(),
        web_contents_, base::Time::Now(), whats_new_registry_.get());
  }

  void TearDown() override {
    mock_storage_service_ = nullptr;
    testing::Test::TearDown();
  }

 protected:
  MockHatsService* mock_hats_service() { return mock_hats_service_; }

  content::BrowserTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
  base::test::ScopedFeatureList feature_list_;
  base::ListValue mock_module_data_;
  raw_ptr<MockWhatsNewStorageService> mock_storage_service_;

  // NOTE: The initialization order of these members matters.
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<MockHatsService> mock_hats_service_;
  content::TestWebContentsFactory factory_;
  raw_ptr<content::WebContents> web_contents_;  // Weak. Owned by factory_.
  std::unique_ptr<WhatsNewRegistry> whats_new_registry_;
  std::unique_ptr<WhatsNewHandler> handler_;
};

TEST_F(WhatsNewHandlerTest, GetServerUrl) {
  base::MockCallback<WhatsNewHandler::GetServerUrlCallback> callback;

  const GURL expected_url =
      GURL(base::StringPrintf("https://www.google.com/chrome/whats-new/"
                              "?version=%d&internal=true",
                              CHROME_VERSION_MAJOR));

  EXPECT_CALL(callback, Run).Times(1).WillOnce([&](GURL actual_url) {
    EXPECT_EQ(actual_url, expected_url);
  });

  handler_->GetServerUrl(false, callback.Get());
}

TEST_F(WhatsNewHandlerTest, GetServerUrlWithCrossDeviceSigninPromo) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({switches::kCrossDeviceSigninFromDesktop,
                             switches::kCrossDeviceSigninFromDesktopFollowup},
                            {});

  // When signed out, `use-native-mobile-qr` is not added.
  {
    base::MockCallback<WhatsNewHandler::GetServerUrlCallback> callback;
    const GURL expected_url =
        GURL(base::StringPrintf("https://www.google.com/chrome/whats-new/"
                                "?version=%d&internal=true",
                                CHROME_VERSION_MAJOR));
    EXPECT_CALL(callback, Run(expected_url)).Times(1);
    handler_->GetServerUrl(false, callback.Get());
  }

  // When signed in, `use-native-mobile-qr` is added.
  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(profile_.get()), "user@gmail.com",
      signin::ConsentLevel::kSignin);
  {
    base::MockCallback<WhatsNewHandler::GetServerUrlCallback> callback;
    const GURL expected_url =
        GURL(base::StringPrintf("https://www.google.com/chrome/whats-new/"
                                "?version=%d&use-native-mobile-qr=true"
                                "&internal=true",
                                CHROME_VERSION_MAJOR));
    EXPECT_CALL(callback, Run(expected_url)).Times(1);
    handler_->GetServerUrl(false, callback.Get());
  }
}

// The page's request to open the native QR bubble is renderer-controlled, so it
// must be ignored, without crashing, unless the page was told to use it.
TEST_F(WhatsNewHandlerTest,
       OpenCrossDeviceSigninQrBubbleIgnoredWhenNotEligible) {
  constexpr char kOpenedHistogram[] =
      "Signin.CrossDeviceSigninPromo.OpenedQrCodeBubble";

  // Feature disabled, before and after the page requested its URL.
  {
    base::test::ScopedFeatureList features;
    features.InitAndDisableFeature(switches::kCrossDeviceSigninFromDesktop);
    handler_->OpenCrossDeviceSigninQrBubble();
    handler_->GetServerUrl(false, base::DoNothing());
    handler_->OpenCrossDeviceSigninQrBubble();
  }

  // Features enabled but signed out.
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures({switches::kCrossDeviceSigninFromDesktop,
                               switches::kCrossDeviceSigninFromDesktopFollowup},
                              {});
    handler_->GetServerUrl(false, base::DoNothing());
    ASSERT_FALSE(handler_->should_show_cross_device_signin_promo_);
    handler_->OpenCrossDeviceSigninQrBubble();
  }

  histogram_tester_.ExpectTotalCount(kOpenedHistogram, 0);
}

TEST_F(WhatsNewHandlerTest, ReloadsOnSignoutWithCrossDeviceSigninPromo) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({switches::kCrossDeviceSigninFromDesktop,
                             switches::kCrossDeviceSigninFromDesktopFollowup},
                            {});
  content::WebContentsTester::For(web_contents_)
      ->NavigateAndCommit(GURL(chrome::kChromeUIWhatsNewURL));
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_.get());
  signin::MakePrimaryAccountAvailable(identity_manager, "user@gmail.com",
                                      signin::ConsentLevel::kSignin);

  // The page was told to rely on the native QR bubble.
  handler_->GetServerUrl(false, base::DoNothing());
  ASSERT_TRUE(handler_->should_show_cross_device_signin_promo_);
  ASSERT_FALSE(web_contents_->GetController().GetPendingEntry());

  // Signing out reloads the page so that it shows its inline QR code again.
  signin::ClearPrimaryAccount(identity_manager);
  EXPECT_FALSE(handler_->should_show_cross_device_signin_promo_);
  EXPECT_TRUE(web_contents_->GetController().GetPendingEntry());
}

TEST_F(WhatsNewHandlerTest, NoReloadOnSignoutWithoutCrossDeviceSigninPromo) {
  // `kCrossDeviceSigninFromDesktopFollowup` is disabled, so the page was not
  // told to rely on the native QR bubble.
  content::WebContentsTester::For(web_contents_)
      ->NavigateAndCommit(GURL(chrome::kChromeUIWhatsNewURL));
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_.get());
  signin::MakePrimaryAccountAvailable(identity_manager, "user@gmail.com",
                                      signin::ConsentLevel::kSignin);
  handler_->GetServerUrl(false, base::DoNothing());

  signin::ClearPrimaryAccount(identity_manager);
  EXPECT_FALSE(web_contents_->GetController().GetPendingEntry());
}

TEST_F(WhatsNewHandlerTest, HistogramsAreEmitted) {
  handler_->RecordTimeToLoadContent(base::Time::Now());
  histogram_tester_.ExpectTotalCount("UserEducation.WhatsNew.TimeToLoadContent",
                                     1);

  handler_->RecordVersionPageLoaded(false);
  EXPECT_EQ(1,
            user_action_tester_.GetActionCount("UserEducation.WhatsNew.Shown"));
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.VersionShown"));
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.ShownByManualNavigation"));

  user_action_tester_.ResetCounts();
  handler_->RecordEditionPageLoaded("NewEdition", false);
  EXPECT_EQ(1,
            user_action_tester_.GetActionCount("UserEducation.WhatsNew.Shown"));
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.EditionShown"));
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.EditionShown.NewEdition"));
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.ShownByManualNavigation"));

  handler_->RecordModuleImpression(
      "MyFeature", whats_new::mojom::ModulePosition::kSpotlight1);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.ModuleShown"));
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.ModuleShown.MyFeature"));
  histogram_tester_.ExpectTotalCount(
      "UserEducation.WhatsNew.ModuleShown.MyFeature", 1);

  handler_->RecordExploreMoreToggled(false);
  histogram_tester_.ExpectTotalCount(
      "UserEducation.WhatsNew.ExploreMoreExpanded", 1);

  handler_->RecordScrollDepth(whats_new::mojom::ScrollDepth::k25);
  histogram_tester_.ExpectTotalCount("UserEducation.WhatsNew.ScrollDepth", 1);

  handler_->RecordTimeOnPage(base::TimeDelta(), true);
  histogram_tester_.ExpectTotalCount("UserEducation.WhatsNew.TimeOnPage", 1);
  histogram_tester_.ExpectTotalCount(
      "UserEducation.WhatsNew.TimeOnPageHeartbeat", 1);

  handler_->RecordModuleLinkClicked(
      "AnotherFeature", whats_new::mojom::ModulePosition::kExploreMore1);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.ModuleLinkClicked"));
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.ModuleLinkClicked.AnotherFeature"));
  histogram_tester_.ExpectTotalCount(
      "UserEducation.WhatsNew.ModuleLinkClicked.AnotherFeature", 1);

  handler_->RecordBrowserCommandExecuted();
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.BrowserCommandExecuted"));

  handler_->RecordModuleVideoStarted(
      "AnotherFeature", whats_new::mojom::ModulePosition::kExploreMore1);
  histogram_tester_.ExpectTotalCount(
      "UserEducation.WhatsNew.VideoStarted.AnotherFeature", 1);

  handler_->RecordModuleVideoEnded(
      "AnotherFeature", whats_new::mojom::ModulePosition::kExploreMore1);
  histogram_tester_.ExpectTotalCount(
      "UserEducation.WhatsNew.VideoEnded.AnotherFeature", 1);

  handler_->RecordModulePlayClicked(
      "AnotherFeature", whats_new::mojom::ModulePosition::kExploreMore1);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.PlayClicked"));
  histogram_tester_.ExpectTotalCount(
      "UserEducation.WhatsNew.PlayClicked.AnotherFeature", 1);

  handler_->RecordModulePauseClicked(
      "AnotherFeature", whats_new::mojom::ModulePosition::kExploreMore1);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.PauseClicked"));
  histogram_tester_.ExpectTotalCount(
      "UserEducation.WhatsNew.PauseClicked.AnotherFeature", 1);

  handler_->RecordModuleRestartClicked(
      "AnotherFeature", whats_new::mojom::ModulePosition::kExploreMore1);
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "UserEducation.WhatsNew.RestartClicked"));
  histogram_tester_.ExpectTotalCount(
      "UserEducation.WhatsNew.RestartClicked.AnotherFeature", 1);
}

TEST_F(WhatsNewHandlerTest, SurveyIsTriggered) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {base::test::FeatureRefAndParams(
          features::kHappinessTrackingSurveysForDesktopWhatsNew,
          {{"whats-new-time", "20s"}})},
      {});

  base::MockCallback<WhatsNewHandler::GetServerUrlCallback> callback;
  EXPECT_CALL(callback, Run).Times(1);
  EXPECT_CALL(*mock_hats_service(),
              LaunchDelayedSurveyForWebContents(kHatsSurveyTriggerWhatsNew, _,
                                                _, _, _, _, _, _, _, _))
      .Times(1);

  handler_->GetServerUrl(false, callback.Get());
}

TEST_F(WhatsNewHandlerTest, SurveyIsTriggeredWithOverride) {
  const std::string survey_override_id = "my-survey-id";
  whats_new_registry_->RegisterEdition(
      whats_new::WhatsNewEdition(kTestEdition, ""));

  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {{kTestEdition, {{whats_new::kSurveyParam, survey_override_id}}},
       base::test::FeatureRefAndParams(
           features::kHappinessTrackingSurveysForDesktopWhatsNew,
           {{"whats-new-time", "20s"}})},
      {});

  base::MockCallback<WhatsNewHandler::GetServerUrlCallback> callback;
  EXPECT_CALL(callback, Run).Times(1);
  EXPECT_CALL(*mock_hats_service(),
              LaunchDelayedSurveyForWebContents(
                  kHatsSurveyTriggerWhatsNew, _, _, _, _, _, _, _,
                  std::optional<std::string>(survey_override_id), _))
      .Times(1);

  handler_->GetServerUrl(false, callback.Get());
}

TEST_F(WhatsNewHandlerTest, SurveyIsNotTriggeredForPreviouslyUsedEdition) {
  const std::string survey_override_id = "my-survey-id";
  whats_new_registry_->RegisterEdition(
      whats_new::WhatsNewEdition(kTestEdition, ""));

  // Mark the registered edition as previously used.
  EXPECT_CALL(*mock_storage_service_, IsUsedEdition)
      .WillRepeatedly(testing::Return(true));

  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {{kTestEdition, {{whats_new::kSurveyParam, survey_override_id}}},
       base::test::FeatureRefAndParams(
           features::kHappinessTrackingSurveysForDesktopWhatsNew,
           {{"whats-new-time", "20s"}})},
      {});

  base::MockCallback<WhatsNewHandler::GetServerUrlCallback> callback;
  EXPECT_CALL(callback, Run).Times(1);
  EXPECT_CALL(*mock_hats_service(),
              LaunchDelayedSurveyForWebContents(kHatsSurveyTriggerWhatsNew, _,
                                                _, _, _, _, _, _, _, _))
      .Times(1);

  handler_->GetServerUrl(false, callback.Get());
}

