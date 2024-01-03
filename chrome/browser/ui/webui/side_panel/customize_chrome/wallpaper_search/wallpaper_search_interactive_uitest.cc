// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/webui/feedback/feedback_dialog.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/url_loader_interceptor.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/views/interaction/interaction_test_util_views.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabPageElementId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kCustomizeChromeElementId);
}  // namespace

class WallpaperSearchInteractiveTest : public InteractiveBrowserTest {
 public:
  WallpaperSearchInteractiveTest() {
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                RegisterMockOptimizationGuideKeyedServiceFactory));
  }
  ~WallpaperSearchInteractiveTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ntp_features::kCustomizeChromeWallpaperSearch,
                              optimization_guide::features::
                                  kOptimizationGuideModelExecution,
                              features::kChromeRefresh2023,
                              features::kChromeWebuiRefresh2023},
        /*disabled_features=*/{});
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
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
                    absl::optional<net::SSLInfo>());
                return true;
              }
              return false;
            }));
  }

  InteractiveTestApi::MultiStep ClickElement(
      const ui::ElementIdentifier& contents_id,
      const DeepQuery& element) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementVisibleEvent);
    StateChange element_visible;
    element_visible.type = StateChange::Type::kExistsAndConditionTrue;
    element_visible.where = element;
    element_visible.event = kElementVisibleEvent;
    element_visible.test_function = "(el) => el.offsetParent !== null";

    return Steps(WaitForStateChange(contents_id, element_visible),
                 MoveMouseTo(contents_id, element), ClickMouse());
  }

  InteractiveTestApi::MultiStep OpenNewTabPage() {
    return Steps(InstrumentTab(kNewTabPageElementId, 0),
                 NavigateWebContents(kNewTabPageElementId,
                                     GURL(chrome::kChromeUINewTabPageURL)),
                 WaitForWebContentsReady(kNewTabPageElementId,
                                         GURL(chrome::kChromeUINewTabPageURL)),
                 Do([this]() {
                   ON_CALL(
                       mock_optimization_guide_keyed_service(),
                       ShouldFeatureBeCurrentlyEnabledForUser(
                           optimization_guide::proto::ModelExecutionFeature::
                               MODEL_EXECUTION_FEATURE_WALLPAPER_SEARCH))
                       .WillByDefault(testing::Return(true));
                 }));
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
    const DeepQuery kEditThemeButton = {
        "customize-chrome-app", "#appearanceElement", "#editThemeButton"};
    const DeepQuery kWallpaperSearchTile = {
        "customize-chrome-app", "#categoriesPage", "#wallpaperSearchTile"};
    return Steps(
        // 1. Open the theme categories page.
        ScrollIntoView(contents_id, kEditThemeButton),
        ClickElement(contents_id, kEditThemeButton),
        // 2. Open Wallpaper Search.
        ClickElement(contents_id, kWallpaperSearchTile));
  }

  InteractiveTestApi::StepBuilder MockWallpaperSearchSuccess() {
    return Do([this]() {
      EXPECT_CALL(
          mock_optimization_guide_keyed_service(),
          ExecuteModel(optimization_guide::proto::ModelExecutionFeature::
                           MODEL_EXECUTION_FEATURE_WALLPAPER_SEARCH,
                       testing::_, testing::_))
          .WillOnce(testing::Invoke(
              [](optimization_guide::proto::ModelExecutionFeature feature_arg,
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
              testing::NiceMock<MockOptimizationGuideKeyedService>>(context);
        }));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::CallbackListSubscription subscription_;
  raw_ptr<testing::NiceMock<MockOptimizationGuideKeyedService>>
      mock_optimization_guide_keyed_service_;
};

IN_PROC_BROWSER_TEST_F(WallpaperSearchInteractiveTest,
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
      // 1. Open Wallpaper Search.
      Steps(OpenNewTabPage(), OpenCustomizeChromeAt(kCustomizeChromeElementId),
            OpenWallpaperSearchAt(kCustomizeChromeElementId)),
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
      // 9. Open wallpaper search.
      OpenWallpaperSearchAt(kReopenedCustomizeChromeElementId),
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
IN_PROC_BROWSER_TEST_F(WallpaperSearchInteractiveTest,
                       FeedbackDialogShowsOnThumbsDown) {
  EXPECT_CALL(mock_optimization_guide_keyed_service(),
              ShouldFeatureBeCurrentlyAllowedForLogging(
                  optimization_guide::proto::ModelExecutionFeature::
                      MODEL_EXECUTION_FEATURE_WALLPAPER_SEARCH))
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
      Steps(OpenNewTabPage(), OpenCustomizeChromeAt(kCustomizeChromeElementId),
            OpenWallpaperSearchAt(kCustomizeChromeElementId)),
      // 2. Show feedback buttons.
      ExecuteJsAt(kCustomizeChromeElementId, kFeedbackButtons,
                  "(el) => el.hidden = false"),
      // 3. Click thumbs down button.
      ClickElement(kCustomizeChromeElementId, kThumbsDown),
      // 4. Ensure that the feedback dialog shows.
      InAnyContext(WaitForShow(FeedbackDialog::kFeedbackDialogForTesting)));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)
