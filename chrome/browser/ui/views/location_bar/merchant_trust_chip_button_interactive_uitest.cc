// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/hats/hats_service_desktop.h"
#include "chrome/browser/ui/views/location_bar/merchant_trust_chip_button_controller.h"
#include "chrome/browser/ui/views/location_bar/omnibox_chip_button.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/browser/ui/views/page_info/page_info_merchant_trust_content_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/commerce/core/proto/merchant_trust.pb.h"
#include "components/optimization_guide/core/hints/optimization_metadata.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/page_info/core/features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondWebContentsElementId);

optimization_guide::OptimizationMetadata GetMerchantTrustMetadata() {
  optimization_guide::OptimizationMetadata optimization_metadata;
  commerce::MerchantTrustSignalsV2 metadata;
  metadata.set_merchant_star_rating(3.5);
  metadata.set_merchant_count_rating(23);
  metadata.set_merchant_details_page_url("https://reviews.test");
  metadata.set_shopper_voice_summary("Test summary");

  optimization_metadata.set_any_metadata(
      optimization_guide::AnyWrapProto(metadata));
  return optimization_metadata;
}

}  // namespace

class MerchantTrustChipButtonInteractiveUITest
    : public InteractiveBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  MerchantTrustChipButtonInteractiveUITest() {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server()->ServeFilesFromSourceDirectory(GetChromeTestDataDir());

    CHECK(https_server()->Start());

    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {page_info::kMerchantTrust,
         {{page_info::kMerchantTrustForceShowUIForTestingName, "true"},
          {page_info::kMerchantTrustEnableOmniboxChipName,
           GetParam() ? "true" : "false"}}},
        {features::kHappinessTrackingSurveysForDesktopDemo, {}},
        {features::kHappinessTrackingSurveysConfiguration,
         {{"custom-url", GetSurveyURL().spec()}}},
        {page_info::kMerchantTrustLearnSurvey,
         {
             {"probability", "1"},
             {"user_prompted", "true"},
             {"trigger_id", "load"},
         }}};
    feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
  }

  ~MerchantTrustChipButtonInteractiveUITest() override = default;
  MerchantTrustChipButtonInteractiveUITest(
      const MerchantTrustChipButtonInteractiveUITest&) = delete;
  void operator=(const MerchantTrustChipButtonInteractiveUITest&) = delete;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    auto* optimization_guide_decider =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile());
    optimization_guide_decider->AddHintForTesting(
        GetURL(), optimization_guide::proto::MERCHANT_TRUST_SIGNALS_V2,
        GetMerchantTrustMetadata());
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch(optimization_guide::switches::
                          kDisableCheckingUserPermissionsForTesting);
  }

  void SetPermission(ContentSettingsType type, ContentSetting setting) {
    HostContentSettingsMap* map =
        HostContentSettingsMapFactory::GetForProfile(browser()->profile());

    map->SetContentSettingDefaultScope(GetURL(), GetURL(), type, setting);
  }

  auto SendKeyPress(ui::KeyboardCode key, bool control, bool shift) {
    return Check([this, key, control, shift]() {
      return ui_test_utils::SendKeyPressSync(browser(), key, control, shift,
                                             false, false);
    });
  }

  auto WasChipAnimatedForWebContents(ElementSpecifier id, bool value) {
    return CheckElement(
        id, base::BindOnce([](ui::TrackedElement* el) {
          return AsInstrumentedWebContents(el)->web_contents()->GetUserData(
                     MerchantTrustChipButtonController::kChipAnimated) !=
                 nullptr;
        }),
        value);
  }

  auto IsChipFullyCollapsed(bool value) {
    return CheckView(kMerchantTrustChipElementId,
                     base::BindOnce([](OmniboxChipButton* view) {
                       return view->is_fully_collapsed();
                     }),
                     value);
  }

  auto CheckHistogramCounts(const std::string& name,
                            auto sample,
                            int expected_count) {
    return Do([=, this]() {
      histogram_tester_.ExpectUniqueSample(name, sample, expected_count);
    });
  }

  MultiStep OpenMerchantTrustSubpage() {
    if (GetParam()) {
      Steps(
          // Open the subpage directly.
          WaitForShow(kMerchantTrustChipElementId),
          PressButton(kMerchantTrustChipElementId),
          WaitForShow(PageInfoMerchantTrustContentView::kElementIdForTesting));
    }

    return Steps(
        PressButton(kLocationIconElementId),
        // Open the page info.
        WaitForShow(PageInfoMainView::kMerchantTrustElementId),
        // Click on the row.
        PressButton(PageInfoMainView::kMerchantTrustElementId),
        // Wait for the subpage to be open.
        WaitForShow(PageInfoMerchantTrustContentView::kElementIdForTesting));
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  GURL GetURL() {
    return https_server()->GetURL("a.test", "/permissions/requests.html");
  }

  GURL GetAnotherURL() {
    return https_server()->GetURL("a.test", "/title1.html");
  }

  GURL GetSurveyURL() {
    return https_server()->GetURL("a.test", "/hats/hats_next_mock.html");
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_P(MerchantTrustChipButtonInteractiveUITest,
                       MerchantTrustChipClick) {
  if (!GetParam()) {
    return;
  }

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      WaitForShow(kMerchantTrustChipElementId),
      PressButton(kMerchantTrustChipElementId),
      WaitForShow(PageInfoMerchantTrustContentView::kElementIdForTesting));
}

IN_PROC_BROWSER_TEST_P(MerchantTrustChipButtonInteractiveUITest,
                       MerchantTrustChipOmniboxEdit) {
  if (!GetParam()) {
    return;
  }
  RunTestSequence(InstrumentTab(kWebContentsElementId),
                  NavigateWebContents(kWebContentsElementId, GetURL()),
                  // The merchant chip is shown.
                  WaitForShow(kMerchantTrustChipElementId),
                  // Start typing.
                  EnterText(kOmniboxElementId, u"query"),
                  // The chip is hidden while typing.
                  WaitForHide(kMerchantTrustChipElementId),
                  // Note: SendAccelerator doesn't work here.
                  // Clear the input.
                  SendKeyPress(ui::VKEY_ESCAPE, false, false),
                  // Exit the editing mode.
                  SendKeyPress(ui::VKEY_ESCAPE, false, false),
                  // The merchant chip is shown again.
                  WaitForShow(kMerchantTrustChipElementId));
}

IN_PROC_BROWSER_TEST_P(MerchantTrustChipButtonInteractiveUITest,
                       LocationBarIconClick) {
  if (!GetParam()) {
    return;
  }
  RunTestSequence(InstrumentTab(kWebContentsElementId),
                  NavigateWebContents(kWebContentsElementId, GetURL()),
                  WaitForShow(kMerchantTrustChipElementId),
                  PressButton(kLocationIconElementId),
                  WaitForShow(PageInfoMainView::kMerchantTrustElementId),
                  EnsurePresent(kMerchantTrustChipElementId));
}

IN_PROC_BROWSER_TEST_P(MerchantTrustChipButtonInteractiveUITest,
                       PermissionRequestOverridesChip) {
  if (!GetParam()) {
    return;
  }
  RunTestSequence(InstrumentTab(kWebContentsElementId),
                  NavigateWebContents(kWebContentsElementId, GetURL()),
                  // The merchant chip is shown.
                  WaitForShow(kMerchantTrustChipElementId),
                  // ...and the permission indicator is not.
                  EnsureNotPresent(PermissionChipView::kElementIdForTesting),
                  // Request notifications.
                  ExecuteJs(kWebContentsElementId, "requestNotification"),
                  // Make sure the request chip is visible.
                  WaitForShow(PermissionChipView::kElementIdForTesting),
                  // ...and the merchant chip is not.
                  WaitForHide(kMerchantTrustChipElementId),
                  // Make sure the permission popup bubble is visible.
                  WaitForShow(PermissionPromptBubbleBaseView::kMainViewId),
                  PressButton(PermissionChipView::kElementIdForTesting),
                  WaitForHide(PermissionPromptBubbleBaseView::kMainViewId),
                  // The permission chip is hidden since the permission request
                  // was dismissed...
                  WaitForHide(PermissionChipView::kElementIdForTesting),
                  // ...and the merchant chip is visible again.
                  WaitForShow(kMerchantTrustChipElementId));
}

IN_PROC_BROWSER_TEST_P(MerchantTrustChipButtonInteractiveUITest,
                       PermissionInUseOverridesChip) {
  if (!GetParam()) {
    return;
  }
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_ALLOW);

  RunTestSequence(InstrumentTab(kWebContentsElementId),
                  NavigateWebContents(kWebContentsElementId, GetURL()),
                  // The merchant chip is shown...
                  WaitForShow(kMerchantTrustChipElementId),
                  // ...and the permission indicator is not.
                  EnsureNotPresent(PermissionChipView::kElementIdForTesting),
                  // Requesting to use the camera (camera is in-use now).
                  ExecuteJs(kWebContentsElementId, "requestCamera"),
                  // Make sure the in-use indicator is visible...
                  WaitForShow(PermissionChipView::kElementIdForTesting),
                  // ...and the merchant chip is not.
                  WaitForHide(kMerchantTrustChipElementId));
}

IN_PROC_BROWSER_TEST_P(MerchantTrustChipButtonInteractiveUITest,
                       AnimateOnlyOncePerTab) {
  if (!GetParam()) {
    return;
  }
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      // The merchant chip is shown and expanded.
      WaitForShow(kMerchantTrustChipElementId),
      WaitForEvent(kMerchantTrustChipElementId, kOmniboxChipButtonExpanded),
      // Animation was recorded.
      WasChipAnimatedForWebContents(kWebContentsElementId, true),
      // Switch to the second tab.
      AddInstrumentedTab(kSecondWebContentsElementId, GetAnotherURL()),
      // The merchant chip is hidden - no merchant trust data for the tab and no
      // animation.
      WaitForHide(kMerchantTrustChipElementId),
      WasChipAnimatedForWebContents(kSecondWebContentsElementId, false),
      // Switch to the first one, the chip was already animated.
      SelectTab(kTabStripElementId, 0),
      WasChipAnimatedForWebContents(kWebContentsElementId, true),
      // The merchant chip is shown again for the first tab but not expanded.
      WaitForShow(kMerchantTrustChipElementId), IsChipFullyCollapsed(true));
}

IN_PROC_BROWSER_TEST_P(MerchantTrustChipButtonInteractiveUITest,
                       MerchantTrustSubpageViewReviews) {
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      OpenMerchantTrustSubpage(),
      CheckView(
          PageInfoMerchantTrustContentView::kViewReviewsId,
          [](RichHoverButton* button) { return button->GetTitleText(); },
          u"View all 23 reviews"),
      // Press the "View all reviews" button.
      PressButton(PageInfoMerchantTrustContentView::kViewReviewsId),
      // Wait for the side panel to show.
      WaitForShow(kSidePanelElementId));
}

IN_PROC_BROWSER_TEST_P(MerchantTrustChipButtonInteractiveUITest,
                       MerchantTrustSubpageHats) {
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      OpenMerchantTrustSubpage(),
      WaitForShow(PageInfoMerchantTrustContentView::kHatsButtonId),
      CheckView(
          PageInfoMerchantTrustContentView::kHatsButtonId,
          [](RichHoverButton* button) { return button->GetTitleText(); },
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_MERCHANT_TRUST_HATS_BUTTON)),
      // Press the HaTS button.
      PressButton(PageInfoMerchantTrustContentView::kHatsButtonId),
      CheckView(
          PageInfoMerchantTrustContentView::kHatsButtonId,
          [](RichHoverButton* button) { return button->GetTitleText(); },
          l10n_util::GetStringUTF16(
              IDS_PAGE_INFO_MERCHANT_TRUST_HATS_LOADING_BUTTON)),
      // Wait for the bubble to be closed and for the survey to show.
      WaitForHide(PageInfoMerchantTrustContentView::kElementIdForTesting),
      InAnyContext(WaitForShow(kHatsNextWebDialogId)),
      CheckHistogramCounts(kHatsShouldShowSurveyReasonHistogram,
                           HatsServiceDesktop::ShouldShowSurveyReasons::kYes,
                           1));
}

INSTANTIATE_TEST_SUITE_P(/*no_prefix*/,
                         MerchantTrustChipButtonInteractiveUITest,
                         testing::Values(false, true));
