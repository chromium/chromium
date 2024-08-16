// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/webui/feedback/feedback_dialog.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/wallpaper_search/wallpaper_search_string_map.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/search/ntp_features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/url_loader_interceptor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/views/interaction/interaction_test_util_views.h"

namespace {
using testing::Return;

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabPageElementId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kCustomizeChromeElementId);

class MockWallpaperSearchStringMap : public WallpaperSearchStringMap {
 public:
  MOCK_CONST_METHOD1(FindCategory,
                     std::optional<std::string>(std::string_view key));
  MOCK_CONST_METHOD1(FindDescriptorA,
                     std::optional<std::string>(std::string_view key));
  MOCK_CONST_METHOD1(FindDescriptorB,
                     std::optional<std::string>(std::string_view key));
  MOCK_CONST_METHOD1(FindDescriptorC,
                     std::optional<std::string>(std::string_view key));
};
}  // namespace

class WallpaperSearchInteractiveTest : public InteractiveBrowserTest {
 public:
  void SetUp() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features =
        GetEnabledFeatures();
    enabled_features.push_back(
        {ntp_features::kNtpWallpaperSearchButtonAnimationShownThreshold,
         {{ntp_features::kNtpWallpaperSearchButtonAnimationShownThresholdParam,
           "15"}}});
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       GetDisabledFeatures());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->profile());
    signin::MakePrimaryAccountAvailable(identity_manager, "user@example.com",
                                        signin::ConsentLevel::kSignin);
  }

  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() {
    return {
        {ntp_features::kCustomizeChromeWallpaperSearch, {}},
        {optimization_guide::features::kOptimizationGuideModelExecution, {}},
        {optimization_guide::features::internal::
             kWallpaperSearchSettingsVisibility,
         {}},
        {ntp_features::kNtpWallpaperSearchButton, {}},
        {ntp_features::kNtpWallpaperSearchButtonAnimation, {}}};
  }

  const std::vector<base::test::FeatureRef> GetDisabledFeatures() {
    return {optimization_guide::features::internal::kWallpaperSearchGraduated};
  }

  InteractiveTestApi::MultiStep WaitForElementExists(
      const ui::ElementIdentifier& contents_id,
      const DeepQuery& element,
      const bool& exists) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementExists);
    StateChange element_exists;
    element_exists.type =
        exists ? StateChange::Type::kExists : StateChange::Type::kDoesNotExist;
    element_exists.where = element;
    element_exists.event = kElementExists;

    return WaitForStateChange(contents_id, element_exists);
  }

  InteractiveTestApi::MultiStep WaitForElementVisible(
      const ui::ElementIdentifier& contents_id,
      const DeepQuery& element) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementVisibleEvent);
    StateChange element_visible;
    element_visible.type = StateChange::Type::kExistsAndConditionTrue;
    element_visible.where = element;
    element_visible.event = kElementVisibleEvent;
    element_visible.test_function = "(el) => el.offsetParent !== null";

    return WaitForStateChange(contents_id, element_visible);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WallpaperSearchInteractiveTest,
                       NTPWallpaperSearchButtonVisibilityDependsOnSettings) {
  const DeepQuery kWallpaperSearchButton = {"ntp-app",
                                            "#wallpaperSearchButton"};

  RunTestSequence(
      // 1. Open the NTP.
      Steps(InstrumentTab(kNewTabPageElementId, 0), Do([=, this]() {
              browser()->profile()->GetPrefs()->SetInteger(
                  optimization_guide::prefs::GetSettingEnabledPrefName(
                      optimization_guide::UserVisibleFeatureKey::
                          kWallpaperSearch),
                  static_cast<int>(
                      optimization_guide::prefs::FeatureOptInState::kEnabled));
            }),
            NavigateWebContents(kNewTabPageElementId,
                                GURL(chrome::kChromeUINewTabPageURL)),
            WaitForWebContentsReady(kNewTabPageElementId,
                                    GURL(chrome::kChromeUINewTabPageURL))),
      // 2. Ensure the wallpaper search button is visible.
      WaitForElementVisible(kNewTabPageElementId, kWallpaperSearchButton),
      // 3. Turn wallpaper search setting off.
      Do([=, this]() {
        browser()->profile()->GetPrefs()->SetInteger(
            optimization_guide::prefs::GetSettingEnabledPrefName(
                optimization_guide::UserVisibleFeatureKey::kWallpaperSearch),
            static_cast<int>(
                optimization_guide::prefs::FeatureOptInState::kDisabled));
      }),
      // 4. Ensure the wallpaper search button is not in the DOM.
      WaitForElementExists(kNewTabPageElementId, kWallpaperSearchButton, false),
      // 5. Turn wallpaper search setting on.
      Do([=, this]() {
        browser()->profile()->GetPrefs()->SetInteger(
            optimization_guide::prefs::GetSettingEnabledPrefName(
                optimization_guide::UserVisibleFeatureKey::kWallpaperSearch),
            static_cast<int>(
                optimization_guide::prefs::FeatureOptInState::kEnabled));
      }),
      // 6. Ensure the wallpaper search button is still not in the DOM.
      WaitForElementExists(kNewTabPageElementId, kWallpaperSearchButton,
                           false));
}

class WallpaperSearchOptimizationGuideInteractiveTest
    : public WallpaperSearchInteractiveTest {
 public:
  WallpaperSearchOptimizationGuideInteractiveTest() {
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                RegisterMockOptimizationGuideKeyedServiceFactory));
  }

  void SetUpOnMainThread() override {
    WallpaperSearchInteractiveTest::SetUpOnMainThread();
    mock_optimization_guide_keyed_service_ =
        static_cast<testing::NiceMock<MockOptimizationGuideKeyedService>*>(
            OptimizationGuideKeyedServiceFactory::GetForProfile(
                browser()->profile()));
    ASSERT_TRUE(mock_optimization_guide_keyed_service_);
  }

  void TearDownOnMainThread() override {
    base::RunLoop().RunUntilIdle();
    if (mock_optimization_guide_keyed_service_) {
      mock_optimization_guide_keyed_service_ = nullptr;
    }
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  std::unique_ptr<content::URLLoaderInterceptor>
  SetUpDescriptorsResponseWithData() {
    // Set up translations.
    auto factory = base::BindRepeating([]() -> std::unique_ptr<
                                                WallpaperSearchStringMap> {
      auto mock =
          std::make_unique<testing::NiceMock<MockWallpaperSearchStringMap>>();
      ON_CALL(*mock, FindCategory("foo")).WillByDefault(Return("foo label"));
      ON_CALL(*mock, FindDescriptorA("bar")).WillByDefault(Return("bar label"));
      ON_CALL(*mock, FindDescriptorB("foo")).WillByDefault(Return("foo label"));
      ON_CALL(*mock, FindDescriptorC("foo")).WillByDefault(Return("foo label"));
      return mock;
    });
    MockWallpaperSearchStringMap::SetFactory(factory);
    return std::make_unique<content::URLLoaderInterceptor>(
        base::BindLambdaForTesting(
            [&](content::URLLoaderInterceptor::RequestParams* params) -> bool {
              if (params->url_request.url.path() ==
                  "/chrome-wallpaper-search/descriptors_en-US.json") {
                std::string headers =
                    "HTTP/1.1 200 OK\nContent-Type: application/json\n\n";
                const std::string body =
                    R"()]}'
                      {
                        "descriptor_a":[
                          {"category":"foo","labels":["bar"]}
                        ],
                        "descriptor_b":[
                          {"label":"foo","image":"bar.png"}
                        ],
                        "descriptor_c":["foo"]
                      })";
                content::URLLoaderInterceptor::WriteResponse(
                    headers, body, params->client.get(),
                    std::optional<net::SSLInfo>());
                return true;
              }
              return false;
            }));
  }

  InteractiveTestApi::MultiStep ClickElement(
      const ui::ElementIdentifier& contents_id,
      const DeepQuery& element) {
    return Steps(WaitForElementVisible(contents_id, element),
                 MoveMouseTo(contents_id, element), ClickMouse());
  }

  InteractiveTestApi::MultiStep OpenNewTabPage() {
    return Steps(
        InstrumentTab(kNewTabPageElementId, 0), Do([this]() {
          ON_CALL(
              mock_optimization_guide_keyed_service(),
              ShouldFeatureBeCurrentlyEnabledForUser(
                  optimization_guide::UserVisibleFeatureKey::kWallpaperSearch))
              .WillByDefault(testing::Return(true));
        }),
        NavigateWebContents(kNewTabPageElementId,
                            GURL(chrome::kChromeUINewTabPageURL)),
        WaitForWebContentsReady(kNewTabPageElementId,
                                GURL(chrome::kChromeUINewTabPageURL)));
  }

  InteractiveTestApi::MultiStep OpenCustomizeChromeAt(
      const ui::ElementIdentifier& contents_id) {
    const DeepQuery kCustomizeChromeButton = {"ntp-app", "#customizeButton"};
    return Steps(ClickElement(kNewTabPageElementId, kCustomizeChromeButton),
                 WaitForShow(kCustomizeChromeSidePanelWebViewElementId),
                 InstrumentNonTabWebView(
                     contents_id, kCustomizeChromeSidePanelWebViewElementId));
  }

  InteractiveTestApi::MultiStep OpenWallpaperSearchAt(
      const ui::ElementIdentifier& contents_id) {
    const DeepQuery kWallpaperSearchButton = {"ntp-app",
                                              "#wallpaperSearchButton"};
    return Steps(ClickElement(kNewTabPageElementId, kWallpaperSearchButton),
                 WaitForShow(kCustomizeChromeSidePanelWebViewElementId),
                 InstrumentNonTabWebView(
                     contents_id, kCustomizeChromeSidePanelWebViewElementId));
  }

  InteractiveTestApi::StepBuilder MockWallpaperSearchSuccess() {
    return Do([this]() {
      EXPECT_CALL(
          mock_optimization_guide_keyed_service(),
          ExecuteModel(
              optimization_guide::ModelBasedCapabilityKey::kWallpaperSearch,
              testing::_, testing::_))
          .WillOnce(testing::Invoke(
              [](optimization_guide::ModelBasedCapabilityKey feature_arg,
                 const google::protobuf::MessageLite& request_arg,
                 optimization_guide::
                     OptimizationGuideModelExecutionResultCallback
                         done_callback_arg) {
                SkBitmap bitmap;
                bitmap.allocN32Pixels(64, 32);
                std::vector<unsigned char> encoded;
                gfx::PNGCodec::EncodeBGRASkBitmap(
                    bitmap, /*discard_transparency=*/false, &encoded);

                optimization_guide::proto::WallpaperSearchResponse response;
                auto* image = response.add_images();
                image->set_encoded_image(
                    std::string(encoded.begin(), encoded.end()));

                std::string serialized_metadata;
                response.SerializeToString(&serialized_metadata);
                optimization_guide::proto::Any result;
                result.set_value(serialized_metadata);
                result.set_type_url("type.googleapis.com/" +
                                    response.GetTypeName());

                std::move(done_callback_arg).Run(base::ok(result), nullptr);
              }));
    });
  }

  MockOptimizationGuideKeyedService& mock_optimization_guide_keyed_service() {
    return *mock_optimization_guide_keyed_service_;
  }

 private:
  static void RegisterMockOptimizationGuideKeyedServiceFactory(
      content::BrowserContext* context) {
    OptimizationGuideKeyedServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<
              testing::NiceMock<MockOptimizationGuideKeyedService>>();
        }));
  }

  raw_ptr<testing::NiceMock<MockOptimizationGuideKeyedService>>
      mock_optimization_guide_keyed_service_;
  base::CallbackListSubscription subscription_;
};

IN_PROC_BROWSER_TEST_F(WallpaperSearchOptimizationGuideInteractiveTest,
                       CustomizeButtonsWorkTogether) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kReopenedCustomizeChromeElementId);

  const DeepQuery kCustomizeChromeButton = {"ntp-app", "#customizeButton"};
  const DeepQuery kWallpaperSearchButton = {"ntp-app",
                                            "#wallpaperSearchButton"};

  RunTestSequence(
      // 1. Open Wallpaper Search with the Wallpaper Search button.
      Steps(OpenNewTabPage(), OpenWallpaperSearchAt(kCustomizeChromeElementId)),
      // 2. Close side panel with the Customize Chrome button.
      Steps(ClickElement(kNewTabPageElementId, kCustomizeChromeButton),
            WaitForHide(kCustomizeChromeSidePanelWebViewElementId)),
      // 3. Reopen the side panel with the Customize Chrome button.
      OpenCustomizeChromeAt(kReopenedCustomizeChromeElementId),
      // 4. Open the wallpaper search page by clicking the Wallpaper Search
      // button.
      ClickElement(kNewTabPageElementId, kWallpaperSearchButton),
      // 5. Close side panel with the Wallpaper Search button.
      Steps(ClickElement(kNewTabPageElementId, kWallpaperSearchButton),
            WaitForHide(kCustomizeChromeSidePanelWebViewElementId)));
}

IN_PROC_BROWSER_TEST_F(WallpaperSearchOptimizationGuideInteractiveTest,
                       NTPButtonAnimatesUnderThreshold) {
  const DeepQuery kCustomizeChromeButton = {"ntp-app", "#customizeButton"};
  const DeepQuery kWallpaperSearchButton = {"ntp-app",
                                            "#wallpaperSearchButton"};

  RunTestSequence(
      // 1. Open the NTP.
      Steps(InstrumentTab(kNewTabPageElementId, 0), Do([this]() {
              ON_CALL(mock_optimization_guide_keyed_service(),
                      ShouldFeatureBeCurrentlyEnabledForUser(
                          optimization_guide::UserVisibleFeatureKey::
                              kWallpaperSearch))
                  .WillByDefault(testing::Return(true));
              // Set shown count lower than threshold.
              browser()->profile()->GetPrefs()->SetInteger(
                  prefs::kNtpWallpaperSearchButtonShownCount, 14);
            }),
            NavigateWebContents(kNewTabPageElementId,
                                GURL(chrome::kChromeUINewTabPageURL)),
            WaitForWebContentsReady(kNewTabPageElementId,
                                    GURL(chrome::kChromeUINewTabPageURL))),
      // 2. Ensure that the wallpaper search button is animated.
      Steps(WaitForElementVisible(kNewTabPageElementId, kWallpaperSearchButton),
            CheckJsResultAt(kNewTabPageElementId, kWallpaperSearchButton,
                            "(el) => el.getAnimations().length > 0")));
}

IN_PROC_BROWSER_TEST_F(WallpaperSearchOptimizationGuideInteractiveTest,
                       NTPButtonDoesNotAnimateAboveThreshold) {
  const DeepQuery kCustomizeChromeButton = {"ntp-app", "#customizeButton"};
  const DeepQuery kWallpaperSearchButton = {"ntp-app",
                                            "#wallpaperSearchButton"};

  RunTestSequence(
      // 1. Open the NTP.
      Steps(InstrumentTab(kNewTabPageElementId, 0), Do([this]() {
              ON_CALL(mock_optimization_guide_keyed_service(),
                      ShouldFeatureBeCurrentlyEnabledForUser(
                          optimization_guide::UserVisibleFeatureKey::
                              kWallpaperSearch))
                  .WillByDefault(testing::Return(true));
              // Set shown count higher than threshold.
              browser()->profile()->GetPrefs()->SetInteger(
                  prefs::kNtpWallpaperSearchButtonShownCount, 16);
            }),
            NavigateWebContents(kNewTabPageElementId,
                                GURL(chrome::kChromeUINewTabPageURL)),
            WaitForWebContentsReady(kNewTabPageElementId,
                                    GURL(chrome::kChromeUINewTabPageURL))),
      // 2. Ensure that the wallpaper search button is not animated.
      Steps(WaitForElementVisible(kNewTabPageElementId, kWallpaperSearchButton),
            CheckJsResultAt(kNewTabPageElementId, kWallpaperSearchButton,
                            "(el) => el.getAnimations().length === 0")));
}

IN_PROC_BROWSER_TEST_F(WallpaperSearchOptimizationGuideInteractiveTest,
                       SearchesAndSetsNewAndHistoricalResults) {
  // Intercept Wallpaper Search descriptor fetches, and respond with data.
  std::unique_ptr<content::URLLoaderInterceptor> descriptors_fetch_interceptor =
      SetUpDescriptorsResponseWithData();

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kReopenedCustomizeChromeElementId);

  const DeepQuery kNewTabPageBody = {"body"};
  const DeepQuery kSubmitButton = {"customize-chrome-app",
                                   "#wallpaperSearchPage", "#wallpaperSearch",
                                   "#submitButton"};
  const DeepQuery kWallpaperSearchResult = {"customize-chrome-app",
                                            "#wallpaperSearchPage", ".result"};
  const DeepQuery kCustomizeChromeButton = {"ntp-app", "#customizeButton"};
  const DeepQuery kSetClassicChromeButton = {
      "customize-chrome-app", "#appearanceElement", "#setClassicChromeButton"};
  const DeepQuery kHistoryCard = {"customize-chrome-app",
                                  "#wallpaperSearchPage", "#historyCard"};
  const DeepQuery kPastResult = {"customize-chrome-app", "#wallpaperSearchPage",
                                 "#historyCard", ".result"};
  const DeepQuery kEditThemeButton = {"customize-chrome-app",
                                      "#appearanceElement", "#editThemeButton"};
  const DeepQuery kWallpaperSearchTile = {
      "customize-chrome-app", "#categoriesPage", "#wallpaperSearchTile"};

  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kNtpHasBackgroundEvent);
  StateChange ntp_has_background;
  ntp_has_background.type = StateChange::Type::kExistsAndConditionTrue;
  ntp_has_background.where = kNewTabPageBody;
  ntp_has_background.event = kNtpHasBackgroundEvent;
  ntp_has_background.test_function =
      "(el) => el.hasAttribute('show-background-image')";

  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kNtpBackgroundResetEvent);
  StateChange ntp_background_reset;
  ntp_background_reset.type = StateChange::Type::kExistsAndConditionTrue;
  ntp_background_reset.where = kNewTabPageBody;
  ntp_background_reset.event = kNtpBackgroundResetEvent;
  ntp_background_reset.test_function =
      "(el) => !el.hasAttribute('show-background-image')";

  RunTestSequence(
      // 1. Open Wallpaper Search via NTP wallpaper search button.
      Steps(OpenNewTabPage(), OpenWallpaperSearchAt(kCustomizeChromeElementId)),
      // 2. Click the submit button.
      //    A random search should trigger, since no descriptors were selected.
      Steps(ClickElement(kCustomizeChromeElementId, kSubmitButton),
            MockWallpaperSearchSuccess()),
      // 3. Click one of the returned wallpapers.
      ClickElement(kCustomizeChromeElementId, kWallpaperSearchResult),
      // 4. Ensure that the NTP has a background.
      WaitForStateChange(kNewTabPageElementId, ntp_has_background),
      // 5. Ensure that there are no past results.
      CheckJsResultAt(kCustomizeChromeElementId, kHistoryCard,
                      "(el) => el.querySelectorAll('.result').length === 0"),
      // 6. Close side panel.
      Steps(ClickElement(kNewTabPageElementId, kCustomizeChromeButton),
            WaitForHide(kCustomizeChromeSidePanelWebViewElementId)),
      // 7. Reopen the side panel.
      OpenCustomizeChromeAt(kReopenedCustomizeChromeElementId),
      // 8. Reset to Classic Chrome.
      Steps(ScrollIntoView(kReopenedCustomizeChromeElementId,
                           kSetClassicChromeButton),
            ClickElement(kReopenedCustomizeChromeElementId,
                         kSetClassicChromeButton),
            WaitForStateChange(kNewTabPageElementId, ntp_background_reset)),
      // 9. Open wallpaper search via themes page.
      Steps(ScrollIntoView(kReopenedCustomizeChromeElementId, kEditThemeButton),
            ClickElement(kReopenedCustomizeChromeElementId, kEditThemeButton),
            ClickElement(kReopenedCustomizeChromeElementId,
                         kWallpaperSearchTile)),
      // 10. Click the past result.
      Steps(CheckJsResultAt(
                kReopenedCustomizeChromeElementId, kHistoryCard,
                "(el) => el.querySelectorAll('.result').length === 1"),
            ClickElement(kReopenedCustomizeChromeElementId, kPastResult)),
      // 11. Ensure that the NTP has a background.
      WaitForStateChange(kNewTabPageElementId, ntp_has_background));
}

// The feedback dialog on CrOS & LaCrOS happens at the system level,
// which cannot be easily tested here. LaCrOS has a separate feedback
// browser test which gives us some coverage.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(WallpaperSearchOptimizationGuideInteractiveTest,
                       FeedbackDialogShowsOnThumbsDown) {
  EXPECT_CALL(mock_optimization_guide_keyed_service(),
              ShouldFeatureBeCurrentlyAllowedForFeedback(
                  optimization_guide::proto::LogAiDataRequest::FeatureCase::
                      kWallpaperSearch))
      .WillOnce(testing::Return(true));

  // Intercept Wallpaper Search descriptor fetches, and respond with data.
  std::unique_ptr<content::URLLoaderInterceptor> descriptors_fetch_interceptor =
      SetUpDescriptorsResponseWithData();

  const DeepQuery kFeedbackButtons = {
      "customize-chrome-app", "#wallpaperSearchPage", "#feedbackButtons"};
  const DeepQuery kThumbsDown = {"customize-chrome-app", "#wallpaperSearchPage",
                                 "#feedbackButtons", "#thumbsDown"};
  RunTestSequence(
      // 1. Open Wallpaper Search.
      Steps(OpenNewTabPage(), OpenWallpaperSearchAt(kCustomizeChromeElementId)),
      // 2. Show feedback buttons.
      ExecuteJsAt(kCustomizeChromeElementId, kFeedbackButtons,
                  "(el) => el.hidden = false"),
      // 3. Click thumbs down button.
      ClickElement(kCustomizeChromeElementId, kThumbsDown),
      // 4. Ensure that the feedback dialog shows.
      InAnyContext(WaitForShow(FeedbackDialog::kFeedbackDialogForTesting)));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(WallpaperSearchOptimizationGuideInteractiveTest,
                       DescriptorErrorCTANavigatesToThemesPage) {
  const DeepQuery kErrorCTA = {"customize-chrome-app", "#wallpaperSearchPage",
                               "#errorCTA"};
  const DeepQuery kThemesPage = {"customize-chrome-app", "#categoriesPage"};
  const DeepQuery kWallpaperSearchTile = {
      "customize-chrome-app", "#categoriesPage", "#wallpaperSearchTile"};
  const DeepQuery kSubmitButton = {"customize-chrome-app",
                                   "#wallpaperSearchPage", "#wallpaperSearch",
                                   "#submitButton"};

  // Intercept Wallpaper Search descriptor fetches, and respond with a network
  // failure or data, depending on the value of |offline|.
  bool offline;
  std::unique_ptr<content::URLLoaderInterceptor> descriptors_fetch_interceptor =
      std::make_unique<content::URLLoaderInterceptor>(
          base::BindLambdaForTesting(
              [&](content::URLLoaderInterceptor::RequestParams* params)
                  -> bool {
                if (params->url_request.url.path() ==
                    "/chrome-wallpaper-search/descriptors_en-US.json") {
                  if (offline) {
                    params->client->OnComplete(
                        network::URLLoaderCompletionStatus(
                            net::ERR_INTERNET_DISCONNECTED));
                  } else {
                    std::string headers =
                        "HTTP/1.1 200 OK\nContent-Type: application/json\n\n";
                    const std::string body =
                        R"()]}'
                      {
                        "descriptor_a":[
                          {"category":"foo","labels":["bar"]}
                        ],
                        "descriptor_b":[
                          {"label":"foo","image":"bar.png"}
                        ],
                        "descriptor_c":["foo"]
                      })";
                    content::URLLoaderInterceptor::WriteResponse(
                        headers, body, params->client.get(),
                        std::optional<net::SSLInfo>());
                  }
                  return true;
                }
                return false;
              }));

  RunTestSequence(
      // 1. Open Wallpaper Search without internet connection.
      Steps(Do(base::BindLambdaForTesting([&]() { offline = true; })),
            OpenNewTabPage(), OpenWallpaperSearchAt(kCustomizeChromeElementId)),
      // 2. Wait for the error CTA to show.
      WaitForElementExists(kCustomizeChromeElementId, kErrorCTA, true),
      WaitForElementVisible(kCustomizeChromeElementId, kErrorCTA),
      // 3. Assert that the themes page isn't showing yet.
      CheckJsResultAt(kCustomizeChromeElementId, kThemesPage,
                      "(el) => el.offsetParent === null"),
      // 4. Click the error CTA.
      ClickElement(kCustomizeChromeElementId, kErrorCTA),
      // 5. Ensure that the themes page shows.
      WaitForElementVisible(kCustomizeChromeElementId, kThemesPage),
      // 6. Reopen Wallpaper Search with internet connection.
      Steps(Do(base::BindLambdaForTesting([&]() { offline = false; })),
            ClickElement(kCustomizeChromeElementId, kWallpaperSearchTile)),
      // 7. Ensure that the error state went away.
      Steps(WaitForElementVisible(kCustomizeChromeElementId, kSubmitButton),
            WaitForElementExists(kCustomizeChromeElementId, kErrorCTA, false)));
}

// Tests in this class should always show an animated button because
// kNtpWallpaperSearchButtonAnimationShownThresholdParam is set to a negative
// value.
class NTPWallpaperSearchButtonAnimationTest
    : public WallpaperSearchOptimizationGuideInteractiveTest {
 public:
  void SetUp() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features =
        GetEnabledFeatures();
    enabled_features.push_back(
        {ntp_features::kNtpWallpaperSearchButtonAnimationShownThreshold,
         {{ntp_features::kNtpWallpaperSearchButtonAnimationShownThresholdParam,
           "-1"}}});
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       GetDisabledFeatures());
    InteractiveBrowserTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(NTPWallpaperSearchButtonAnimationTest,
                       AnimatesUnconditionally) {
  const DeepQuery kCustomizeChromeButton = {"ntp-app", "#customizeButton"};
  const DeepQuery kWallpaperSearchButton = {"ntp-app",
                                            "#wallpaperSearchButton"};

  RunTestSequence(
      // 1. Open the NTP.
      Steps(InstrumentTab(kNewTabPageElementId, 0), Do([this]() {
              ON_CALL(mock_optimization_guide_keyed_service(),
                      ShouldFeatureBeCurrentlyEnabledForUser(
                          optimization_guide::UserVisibleFeatureKey::
                              kWallpaperSearch))
                  .WillByDefault(testing::Return(true));
              // Set shown count higher than threshold.
              browser()->profile()->GetPrefs()->SetInteger(
                  prefs::kNtpWallpaperSearchButtonShownCount, 1000);
            }),
            NavigateWebContents(kNewTabPageElementId,
                                GURL(chrome::kChromeUINewTabPageURL)),
            WaitForWebContentsReady(kNewTabPageElementId,
                                    GURL(chrome::kChromeUINewTabPageURL))),
      // 2. Ensure that the wallpaper search button is animated.
      Steps(WaitForElementVisible(kNewTabPageElementId, kWallpaperSearchButton),
            CheckJsResultAt(kNewTabPageElementId, kWallpaperSearchButton,
                            "(el) => el.getAnimations().length > 0")));
}
