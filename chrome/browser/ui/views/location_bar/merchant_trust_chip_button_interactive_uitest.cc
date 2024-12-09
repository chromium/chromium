// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/location_bar/merchant_trust_chip_button_controller.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/browser/ui/views/page_info/page_info_merchant_trust_content_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/commerce/core/proto/merchant_trust.pb.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/page_info/core/features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);

optimization_guide::OptimizationMetadata GetMerchantTrustMetadata() {
  optimization_guide::OptimizationMetadata optimization_metadata;
  commerce::MerchantTrustSignalsV2 metadata;
  metadata.set_merchant_star_rating(3.5);
  metadata.set_merchant_count_rating(23);
  metadata.set_merchant_details_page_url("https://reviews.test");
  metadata.set_reviews_summary("Test summary");

  optimization_metadata.SetAnyMetadataForTesting(metadata);
  return optimization_metadata;
}

}  // namespace

class MerchantTrustChipButtonInteractiveUITest : public InteractiveBrowserTest {
 public:
  MerchantTrustChipButtonInteractiveUITest() {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {page_info::kMerchantTrust,
         {{page_info::kMerchantTrustForceShowUIForTestingName, "true"}}}};
    feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
  }

  ~MerchantTrustChipButtonInteractiveUITest() override = default;
  MerchantTrustChipButtonInteractiveUITest(
      const MerchantTrustChipButtonInteractiveUITest&) = delete;
  void operator=(const MerchantTrustChipButtonInteractiveUITest&) = delete;

  void SetUp() override {
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server()->ServeFilesFromSourceDirectory(GetChromeTestDataDir());

    ASSERT_TRUE(https_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server()->StartAcceptingConnections();

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

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  GURL GetURL() {
    return https_server()->GetURL("a.test", "/permissions/requests.html");
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(MerchantTrustChipButtonInteractiveUITest,
                       MerchantTrustChipClick) {
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      WaitForShow(MerchantTrustChipButtonController::kElementIdForTesting),
      PressButton(MerchantTrustChipButtonController::kElementIdForTesting),
      WaitForShow(PageInfoMerchantTrustContentView::kElementIdForTesting));
}

IN_PROC_BROWSER_TEST_F(MerchantTrustChipButtonInteractiveUITest,
                       MerchantTrustChipOmniboxEdit) {
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      // The merchant chip is shown.
      WaitForShow(MerchantTrustChipButtonController::kElementIdForTesting),
      // Start typing.
      EnterText(kOmniboxElementId, u"query"),
      // The chip is hidden while typing.
      WaitForHide(MerchantTrustChipButtonController::kElementIdForTesting),
      // Note: SendAccelerator doesn't work here.
      // Clear the input.
      SendKeyPress(ui::VKEY_ESCAPE, false, false),
      // Exit the editing mode.
      SendKeyPress(ui::VKEY_ESCAPE, false, false),
      // The merchant chip is shown again.
      WaitForShow(MerchantTrustChipButtonController::kElementIdForTesting));
}

IN_PROC_BROWSER_TEST_F(MerchantTrustChipButtonInteractiveUITest,
                       LocationBarIconClick) {
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      WaitForShow(MerchantTrustChipButtonController::kElementIdForTesting),
      PressButton(kLocationIconElementId),
      WaitForShow(PageInfoMainView::kMerchantTrustElementId),
      EnsurePresent(MerchantTrustChipButtonController::kElementIdForTesting));
}

IN_PROC_BROWSER_TEST_F(MerchantTrustChipButtonInteractiveUITest,
                       PermissionRequestOverridesChip) {
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      // The merchant chip is shown.
      WaitForShow(MerchantTrustChipButtonController::kElementIdForTesting),
      // ...and the permission indicator is not.
      EnsureNotPresent(PermissionChipView::kElementIdForTesting),
      // Request notifications.
      ExecuteJs(kWebContentsElementId, "requestNotification"),
      // Make sure the request chip is visible.
      WaitForShow(PermissionChipView::kElementIdForTesting),
      // ...and the merchant chip is not.
      WaitForHide(MerchantTrustChipButtonController::kElementIdForTesting),
      // Make sure the permission popup bubble is visible.
      WaitForShow(PermissionPromptBubbleBaseView::kMainViewId),
      PressButton(PermissionChipView::kElementIdForTesting),
      WaitForHide(PermissionPromptBubbleBaseView::kMainViewId),
      // The permission chip is hidden since the permission request was
      // dismissed...
      WaitForHide(PermissionChipView::kElementIdForTesting),
      // ...and the merchant chip is visible again.
      WaitForShow(MerchantTrustChipButtonController::kElementIdForTesting));
}

IN_PROC_BROWSER_TEST_F(MerchantTrustChipButtonInteractiveUITest,
                       PermissionInUseOverridesChip) {
  SetPermission(ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_ALLOW);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      // The merchant chip is shown...
      WaitForShow(MerchantTrustChipButtonController::kElementIdForTesting),
      // ...and the permission indicator is not.
      EnsureNotPresent(PermissionChipView::kElementIdForTesting),
      // Requesting to use the camera (camera is in-use now).
      ExecuteJs(kWebContentsElementId, "requestCamera"),
      // Make sure the in-use indicator is visible...
      WaitForShow(PermissionChipView::kElementIdForTesting),
      // ...and the merchant chip is not.
      WaitForHide(MerchantTrustChipButtonController::kElementIdForTesting));
}
