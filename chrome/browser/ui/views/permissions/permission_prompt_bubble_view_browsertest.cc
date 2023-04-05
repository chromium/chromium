// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_view.h"

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/download/download_permission_request.h"
#include "chrome/browser/permissions/attestation_permission_request.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/permissions/chip_controller.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_chip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/custom_handlers/register_protocol_handler_permission_request.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_ui_selector.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_request.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/cursor_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/button_test_api.h"

namespace {
// Test implementation of PermissionUiSelector that always returns a canned
// decision.
class TestQuietNotificationPermissionUiSelector
    : public permissions::PermissionUiSelector {
 public:
  explicit TestQuietNotificationPermissionUiSelector(
      const Decision& canned_decision)
      : canned_decision_(canned_decision) {}
  ~TestQuietNotificationPermissionUiSelector() override = default;

 protected:
  // permissions::PermissionUiSelector:
  void SelectUiToUse(permissions::PermissionRequest* request,
                     DecisionMadeCallback callback) override {
    std::move(callback).Run(canned_decision_);
  }

  bool IsPermissionRequestSupported(
      permissions::RequestType request_type) override {
    return request_type == permissions::RequestType::kNotifications;
  }

 private:
  Decision canned_decision_;
};
}  // namespace

class PermissionPromptBubbleViewBrowserTest
    : public DialogBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  PermissionPromptBubbleViewBrowserTest() {
    if (GetParam()) {
      feature_list_.InitWithFeatures({permissions::features::kPermissionChip},
                                     {});
    } else {
      feature_list_.InitWithFeatures({},
                                     {permissions::features::kPermissionChip});
    }
  }

  PermissionPromptBubbleViewBrowserTest(
      const PermissionPromptBubbleViewBrowserTest&) = delete;
  PermissionPromptBubbleViewBrowserTest& operator=(
      const PermissionPromptBubbleViewBrowserTest&) = delete;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("a.com", "/empty.html")));

    test_api_ =
        std::make_unique<test::PermissionRequestManagerTestApi>(browser());
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    const std::string& actual_name = name.substr(0, name.find("/"));
    if (actual_name == "security_key") {
      // This one doesn't have a ContentSettingsType.
      test_api_->manager()->AddRequest(
          GetActiveMainFrame(),
          NewAttestationPermissionRequest(url::Origin::Create(GetTestUrl()),
                                          base::BindOnce([](bool) {})));
    } else {
      AddRequestForContentSetting(actual_name);
    }
    base::RunLoop().RunUntilIdle();

    ChipController* chip_controller = GetChipController();
    if (chip_controller->IsPermissionPromptChipVisible()) {
      views::test::ButtonTestApi(chip_controller->chip())
          .NotifyClick(ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                      gfx::Point(), ui::EventTimeForNow(),
                                      ui::EF_LEFT_MOUSE_BUTTON, 0));
      base::RunLoop().RunUntilIdle();
    }
  }

  GURL GetTestUrl() { return GURL("https://example.com"); }

  content::RenderFrameHost* GetActiveMainFrame() {
    return browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetPrimaryMainFrame();
  }

  ChipController* GetChipController() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->toolbar()->location_bar()->chip_controller();
  }

  ContentSettingImageView& GetContentSettingImageView(
      ContentSettingImageModel::ImageType image_type) {
    LocationBarView* location_bar_view =
        BrowserView::GetBrowserViewForBrowser(browser())->GetLocationBarView();
    return **base::ranges::find(
        location_bar_view->GetContentSettingViewsForTest(), image_type,
        &ContentSettingImageView::GetTypeForTesting);
  }

  permissions::PermissionRequest* MakeRegisterProtocolHandlerRequest() {
    std::string protocol = "mailto";
    custom_handlers::ProtocolHandler handler =
        custom_handlers::ProtocolHandler::CreateProtocolHandler(protocol,
                                                                GetTestUrl());
    custom_handlers::ProtocolHandlerRegistry* registry =
        ProtocolHandlerRegistryFactory::GetForBrowserContext(
            browser()->profile());
    // Deleted in RegisterProtocolHandlerPermissionRequest::RequestFinished().
    return new custom_handlers::RegisterProtocolHandlerPermissionRequest(
        registry, handler, GetTestUrl(), base::ScopedClosureRunner());
  }

  void AddRequestForContentSetting(const std::string& name) {
    constexpr const char* kMultipleName = "multiple";
    constexpr struct {
      const char* name;
      ContentSettingsType type;
    } kNameToType[] = {
        {"geolocation", ContentSettingsType::GEOLOCATION},
        {"protected_media", ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER},
        {"notifications", ContentSettingsType::NOTIFICATIONS},
        {"mic", ContentSettingsType::MEDIASTREAM_MIC},
        {"camera", ContentSettingsType::MEDIASTREAM_CAMERA},
        {"protocol_handlers", ContentSettingsType::PROTOCOL_HANDLERS},
        {"midi", ContentSettingsType::MIDI_SYSEX},
        {"storage_access", ContentSettingsType::STORAGE_ACCESS},
        {"downloads", ContentSettingsType::AUTOMATIC_DOWNLOADS},
        {kMultipleName, ContentSettingsType::DEFAULT}};
    const auto* it = std::begin(kNameToType);
    for (; it != std::end(kNameToType); ++it) {
      if (name == it->name)
        break;
    }
    if (it == std::end(kNameToType)) {
      ADD_FAILURE() << "Unknown: " << name;
      return;
    }
    permissions::PermissionRequestManager* manager = test_api_->manager();
    content::RenderFrameHost* source_frame = GetActiveMainFrame();
    switch (it->type) {
      case ContentSettingsType::PROTOCOL_HANDLERS:
        manager->AddRequest(source_frame, MakeRegisterProtocolHandlerRequest());
        break;
      case ContentSettingsType::AUTOMATIC_DOWNLOADS:
        manager->AddRequest(source_frame,
                            new DownloadPermissionRequest(
                                nullptr, url::Origin::Create(GetTestUrl())));
        break;
      case ContentSettingsType::DURABLE_STORAGE:
        // TODO(tapted): Prompt for quota request.
        break;
      case ContentSettingsType::MEDIASTREAM_MIC:
      case ContentSettingsType::MEDIASTREAM_CAMERA:
      case ContentSettingsType::MIDI_SYSEX:
      case ContentSettingsType::NOTIFICATIONS:
      case ContentSettingsType::GEOLOCATION:
      case ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER:  // ChromeOS only.
      case ContentSettingsType::STORAGE_ACCESS:
        test_api_->AddSimpleRequest(
            source_frame,
            permissions::ContentSettingsTypeToRequestType(it->type));
        break;
      case ContentSettingsType::DEFAULT:
        // Permissions to request for a "multiple" request. Only mic/camera
        // requests are grouped together.
        EXPECT_EQ(kMultipleName, name);
        test_api_->AddSimpleRequest(source_frame,
                                    permissions::RequestType::kMicStream);
        test_api_->AddSimpleRequest(source_frame,
                                    permissions::RequestType::kCameraStream);
        break;
      default:
        ADD_FAILURE() << "Not a permission type, or one that doesn't prompt.";
        return;
    }
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<test::PermissionRequestManagerTestApi> test_api_;
};

IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewBrowserTest,
                       AlertAccessibleEvent) {
  views::test::AXEventCounter counter(views::AXEventManager::Get());
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kAlert));
  ShowUi("geolocation");

  ChipController* chip_controller = GetChipController();

  // If chip UI is used, two notifications will be announced: one that
  // permission was requested and second when bubble is opened.
  if (chip_controller->IsPermissionPromptChipVisible()) {
    EXPECT_EQ(2, counter.GetCount(ax::mojom::Event::kAlert));
  } else {
    EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kAlert));
  }
}

// Test switching between PermissionChip and PermissionPromptBubbleView and
// make sure no crashes.
IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewBrowserTest,
                       SwitchBetweenChipAndBubble) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  browser_view->GetLocationBarView()->SetVisible(false);
  ShowUi("geolocation");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);
  permission_request_manager->UpdateAnchor();
  browser_view->GetLocationBarView()->SetVisible(true);
  permission_request_manager->UpdateAnchor();
  browser_view->GetLocationBarView()->SetVisible(false);
  permission_request_manager->UpdateAnchor();
}

// Test bubbles showing when tabs move between windows. Simulates a situation
// that could result in permission bubbles not being dismissed, and a problem
// referencing a temporary drag window. See http://crbug.com/754552.
#if BUILDFLAG(IS_MAC)
#define MAYBE_SwitchBrowserWindow DISABLED_SwitchBrowserWindow
#else
#define MAYBE_SwitchBrowserWindow SwitchBrowserWindow
#endif
IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewBrowserTest,
                       MAYBE_SwitchBrowserWindow) {
  ShowUi("geolocation");
  TabStripModel* strip = browser()->tab_strip_model();

  // Drag out into a dragging window. E.g. see steps in [BrowserWindowController
  // detachTabsToNewWindow:..].
  std::vector<TabStripModelDelegate::NewStripContents> contentses(1);
  contentses.back().add_types = AddTabTypes::ADD_ACTIVE;
  contentses.back().web_contents = strip->DetachWebContentsAtForInsertion(0);
  Browser* dragging_browser = strip->delegate()->CreateNewStripWithContents(
      std::move(contentses), gfx::Rect(100, 100, 640, 480), false);

  // Attach the tab back to the original window. E.g. See steps in
  // [BrowserWindowController moveTabViews:..].
  TabStripModel* drag_strip = dragging_browser->tab_strip_model();
  std::unique_ptr<content::WebContents> removed_contents =
      drag_strip->DetachWebContentsAtForInsertion(0);
  strip->InsertWebContentsAt(0, std::move(removed_contents),
                             AddTabTypes::ADD_ACTIVE);

  // Clear the request. There should be no crash.
  test_api_->SimulateWebContentsDestroyed();
}

// crbug.com/989858
#if BUILDFLAG(IS_WIN)
#define MAYBE_ActiveTabClosedAfterRendererCrashesWithPendingPermissionRequest \
  DISABLED_ActiveTabClosedAfterRendererCrashesWithPendingPermissionRequest
#else
#define MAYBE_ActiveTabClosedAfterRendererCrashesWithPendingPermissionRequest \
  ActiveTabClosedAfterRendererCrashesWithPendingPermissionRequest
#endif
// Regression test for https://crbug.com/933321.
IN_PROC_BROWSER_TEST_P(
    PermissionPromptBubbleViewBrowserTest,
    MAYBE_ActiveTabClosedAfterRendererCrashesWithPendingPermissionRequest) {
  ShowUi("geolocation");
  ASSERT_TRUE(VerifyUi());

  // Simulate a render process crash while the permission prompt is pending.
  content::RenderViewHost* render_view_host = browser()
                                                  ->tab_strip_model()
                                                  ->GetActiveWebContents()
                                                  ->GetPrimaryMainFrame()
                                                  ->GetRenderViewHost();
  content::RenderProcessHost* render_process_host =
      render_view_host->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      render_process_host,
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  ASSERT_TRUE(render_process_host->Shutdown(0));
  crash_observer.Wait();

  // The permission request is still pending, but the BrowserView's WebView is
  // now showing a crash overlay, so the permission prompt is hidden.
  //
  // Now close the tab. This will first detach the WebContents, causing the
  // WebView's crash overlay to be torn down, which, in turn, will temporarily
  // make the dying WebContents visible again, albeit without being attached to
  // any BrowserView.
  //
  // Wait until the WebContents, and with it, the PermissionRequestManager, is
  // gone, and make sure nothing crashes.
  content::WebContentsDestroyedWatcher web_contents_destroyed_watcher(
      browser()->tab_strip_model()->GetActiveWebContents());
  browser()->tab_strip_model()->CloseAllTabs();
  web_contents_destroyed_watcher.Wait();
}

// Host wants to know your location.
IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewBrowserTest,
                       InvokeUi_geolocation) {
  ShowAndVerifyUi();
}

// Host wants to show notifications.
IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewBrowserTest,
                       InvokeUi_notifications) {
  ShowAndVerifyUi();
}

// Host wants to use your microphone.
IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewBrowserTest, InvokeUi_mic) {
  ShowAndVerifyUi();
}

// Host wants to use your camera.
IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewBrowserTest, InvokeUi_camera) {
  ShowAndVerifyUi();
}

// Host wants to open email links.
IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewBrowserTest,
                       InvokeUi_protocol_handlers) {
  ShowAndVerifyUi();
}

// Host wants to use your MIDI devices.
IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewBrowserTest, InvokeUi_midi) {
  ShowAndVerifyUi();
}

// TODO(crbug.com/1232028): Pixel verification for storage_access test checks
// permission request prompt that has origin and port. Because these tests run
// on localhost, the port constantly changes its value and hence test pixel
// verification fails. Host wants to access storage from the site in which it's
// embedded.
// Host wants to access storage from the site in which it's embedded.
IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewBrowserTest,
                       DISABLED_InvokeUi_storage_access) {
  ShowAndVerifyUi();
}

// Host wants to trigger multiple downloads.
IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewBrowserTest,
                       InvokeUi_downloads) {
  ShowAndVerifyUi();
}

// Host wants to access data about your security key.
IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewBrowserTest,
                       InvokeUi_security_key) {
  ShowAndVerifyUi();
}

// Shows a permissions bubble with multiple requests.
IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewBrowserTest,
                       InvokeUi_multiple) {
  ShowAndVerifyUi();
}

class QuietUIPromoBrowserTest : public PermissionPromptBubbleViewBrowserTest {
 public:
  QuietUIPromoBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kQuietNotificationPrompts,
          {{QuietNotificationPermissionUiConfig::kEnableAdaptiveActivation,
            "true"}}}},
        {{permissions::features::kPermissionQuietChip}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(QuietUIPromoBrowserTest, InvokeUi_QuietUIPromo) {
  auto* profile = browser()->profile();
  // Promo is not enabled by default.
  EXPECT_FALSE(QuietNotificationPermissionUiState::ShouldShowPromo(profile));

  for (const char* origin_spec :
       {"https://a.com", "https://b.com", "https://c.com"}) {
    GURL requesting_origin(origin_spec);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), requesting_origin));
    permissions::MockPermissionRequest notification_request(
        requesting_origin, permissions::RequestType::kNotifications);
    test_api_->manager()->AddRequest(GetActiveMainFrame(),
                                     &notification_request);
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(test_api_->manager()->ShouldCurrentRequestUseQuietUI());
    EXPECT_FALSE(QuietNotificationPermissionUiState::ShouldShowPromo(profile));
    test_api_->manager()->Deny();
    base::RunLoop().RunUntilIdle();
  }

  ContentSettingImageView& quiet_ui_icon = GetContentSettingImageView(
      ContentSettingImageModel::ImageType::NOTIFICATIONS_QUIET_PROMPT);

  EXPECT_FALSE(quiet_ui_icon.GetVisible());
  // `ContentSettingImageView::AnimationEnded()` was not triggered and IPH is
  // not shown.
  EXPECT_FALSE(quiet_ui_icon.critical_promo_bubble_for_testing());

  GURL notification("http://www.notification1.com/");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), notification));
  permissions::MockPermissionRequest notification_request(
      notification, permissions::RequestType::kNotifications);
  test_api_->manager()->AddRequest(GetActiveMainFrame(), &notification_request);
  base::RunLoop().RunUntilIdle();

  // After 3 denied Notifications requests, Adaptive activation enabled quiet
  // permission prompt.
  EXPECT_TRUE(test_api_->manager()->ShouldCurrentRequestUseQuietUI());
  // At the first quiet permission prompt we show IPH.
  ASSERT_TRUE(QuietNotificationPermissionUiState::ShouldShowPromo(profile));

  EXPECT_TRUE(quiet_ui_icon.GetVisible());
  EXPECT_TRUE(quiet_ui_icon.is_animating_label());
  // Animation is reset to trigger `ContentSettingImageView::AnimationEnded()`.
  // `AnimationEnded` contains logic for displaying IPH and marking it as shown.
  quiet_ui_icon.reset_animation_for_testing();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(quiet_ui_icon.is_animating_label());

  // The IPH is showing.
  auto* help_bubble = quiet_ui_icon.critical_promo_bubble_for_testing();
  ASSERT_TRUE(help_bubble && help_bubble->is_open());
  auto* const iph_controller = BrowserView::GetBrowserViewForBrowser(browser())
                                   ->GetFeaturePromoController();
  // The critical promo that is currently showing is the one created by a quiet
  // permission prompt.
  EXPECT_EQ(help_bubble, iph_controller->critical_promo_bubble_for_testing());

  help_bubble->Close();

  test_api_->manager()->Deny();
  base::RunLoop().RunUntilIdle();

  // After quiet permission prompt was resolved, the critical promo is reset.
  EXPECT_FALSE(quiet_ui_icon.critical_promo_bubble_for_testing());

  EXPECT_FALSE(quiet_ui_icon.GetVisible());

  // The second Notifications permission request to verify that the IPH is not
  // shown.
  GURL notification2("http://www.notification2.com/");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), notification2));
  permissions::MockPermissionRequest notification_request2(
      notification2, permissions::RequestType::kNotifications);
  test_api_->manager()->AddRequest(GetActiveMainFrame(),
                                   &notification_request2);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_api_->manager()->ShouldCurrentRequestUseQuietUI());
  // At the second quiet permission prompt the IPH should be disabled.
  EXPECT_FALSE(QuietNotificationPermissionUiState::ShouldShowPromo(profile));

  EXPECT_TRUE(quiet_ui_icon.GetVisible());
  EXPECT_TRUE(quiet_ui_icon.is_animating_label());
  quiet_ui_icon.reset_animation_for_testing();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(quiet_ui_icon.is_animating_label());

  // The critical promo is not shown.
  EXPECT_FALSE(quiet_ui_icon.critical_promo_bubble_for_testing());
  EXPECT_FALSE(iph_controller->critical_promo_bubble_for_testing());

  test_api_->manager()->Deny();
  base::RunLoop().RunUntilIdle();
}

// ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER is ChromeOS only.
#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewBrowserTest,
                       InvokeUi_protected_media) {
  ShowAndVerifyUi();
}
#endif

// Test that the quiet prompt disposition returns the same value when permission
// is not considered abusive (currently only applicable for Notifications) vs.
// when permission is not considered abusive.
IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewBrowserTest,
                       DispositionNoAbusiveTest) {
  base::HistogramTester histograms;

  ShowUi("geolocation");

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      GetParam() ? permissions::PermissionPromptDisposition::
                       LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE
                 : permissions::PermissionPromptDisposition::ANCHORED_BUBBLE);

  base::TimeDelta duration = base::Milliseconds(42);
  test_api_->manager()->set_time_to_decision_for_test(duration);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  if (GetParam()) {
    histograms.ExpectBucketCount(
        "Permissions.Prompt.Geolocation.LocationBarLeftChipAutoBubble.Action",
        static_cast<int>(permissions::PermissionAction::GRANTED), 1);
    histograms.ExpectTimeBucketCount(
        "Permissions.Prompt.Geolocation.LocationBarLeftChipAutoBubble.Accepted."
        "TimeToAction",
        duration, 1);
  } else {
    histograms.ExpectBucketCount(
        "Permissions.Prompt.Geolocation.AnchoredBubble.Action",
        static_cast<int>(permissions::PermissionAction::GRANTED), 1);
    histograms.ExpectTimeBucketCount(
        "Permissions.Prompt.Geolocation.AnchoredBubble.Accepted."
        "TimeToAction",
        duration, 1);
  }

  ShowUi("notifications");

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      GetParam() ? permissions::PermissionPromptDisposition::
                       LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE
                 : permissions::PermissionPromptDisposition::ANCHORED_BUBBLE);

  duration = base::Milliseconds(42);
  test_api_->manager()->set_time_to_decision_for_test(duration);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  if (GetParam()) {
    histograms.ExpectBucketCount(
        "Permissions.Prompt.Notifications.LocationBarLeftChipAutoBubble.Action",
        static_cast<int>(permissions::PermissionAction::GRANTED), 1);
    histograms.ExpectTimeBucketCount(
        "Permissions.Prompt.Notifications.LocationBarLeftChipAutoBubble."
        "Accepted."
        "TimeToAction",
        duration, 1);
  } else {
    histograms.ExpectBucketCount(
        "Permissions.Prompt.Notifications.AnchoredBubble.Action",
        static_cast<int>(permissions::PermissionAction::GRANTED), 1);
    histograms.ExpectTimeBucketCount(
        "Permissions.Prompt.Notifications.AnchoredBubble.Accepted."
        "TimeToAction",
        duration, 1);
  }
}

IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewBrowserTest,
                       AcceptedOnceDispositionNoAbusiveTest) {
  base::HistogramTester histograms;

  ShowUi("geolocation");

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      GetParam() ? permissions::PermissionPromptDisposition::
                       LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE
                 : permissions::PermissionPromptDisposition::ANCHORED_BUBBLE);

  base::TimeDelta duration = base::Milliseconds(42);
  test_api_->manager()->set_time_to_decision_for_test(duration);

  test_api_->manager()->AcceptThisTime();
  base::RunLoop().RunUntilIdle();

  if (GetParam()) {
    histograms.ExpectBucketCount(
        "Permissions.Prompt.Geolocation.LocationBarLeftChipAutoBubble.Action",
        static_cast<int>(permissions::PermissionAction::GRANTED_ONCE), 1);
    histograms.ExpectTimeBucketCount(
        "Permissions.Prompt.Geolocation.LocationBarLeftChipAutoBubble."
        "AcceptedOnce."
        "TimeToAction",
        duration, 1);
  } else {
    histograms.ExpectBucketCount(
        "Permissions.Prompt.Geolocation.AnchoredBubble.Action",
        static_cast<int>(permissions::PermissionAction::GRANTED_ONCE), 1);
    histograms.ExpectTimeBucketCount(
        "Permissions.Prompt.Geolocation.AnchoredBubble.AcceptedOnce."
        "TimeToAction",
        duration, 1);
  }

  ShowUi("notifications");

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      GetParam() ? permissions::PermissionPromptDisposition::
                       LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE
                 : permissions::PermissionPromptDisposition::ANCHORED_BUBBLE);

  duration = base::Milliseconds(42);
  test_api_->manager()->set_time_to_decision_for_test(duration);

  test_api_->manager()->AcceptThisTime();
  base::RunLoop().RunUntilIdle();

  if (GetParam()) {
    histograms.ExpectBucketCount(
        "Permissions.Prompt.Notifications.LocationBarLeftChipAutoBubble.Action",
        static_cast<int>(permissions::PermissionAction::GRANTED_ONCE), 1);
    histograms.ExpectTimeBucketCount(
        "Permissions.Prompt.Notifications.LocationBarLeftChipAutoBubble."
        "AcceptedOnce."
        "TimeToAction",
        duration, 1);
  } else {
    histograms.ExpectBucketCount(
        "Permissions.Prompt.Notifications.AnchoredBubble.Action",
        static_cast<int>(permissions::PermissionAction::GRANTED_ONCE), 1);
    histograms.ExpectTimeBucketCount(
        "Permissions.Prompt.Notifications.AnchoredBubble.AcceptedOnce."
        "TimeToAction",
        duration, 1);
  }
}

IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewBrowserTest,
                       PermissionPromptBubbleDisallowsCustomCursors) {
  ui::Cursor custom_cursor(ui::mojom::CursorType::kCustom);

  content::RenderWidgetHost* widget_host = test_api_->manager()
                                               ->GetAssociatedWebContents()
                                               ->GetRenderViewHost()
                                               ->GetWidget();

  // Initially custom cursors are allowed.
  widget_host->SetCursor(custom_cursor);
  EXPECT_EQ(content::CursorUtils::GetLastCursorForWebContents(
                test_api_->manager()->GetAssociatedWebContents()),
            ui::mojom::CursorType::kCustom);

  // While a permission prompt is active custom cursors are not allowed.
  ShowUi("geolocation");
  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      GetParam() ? permissions::PermissionPromptDisposition::
                       LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE
                 : permissions::PermissionPromptDisposition::ANCHORED_BUBBLE);

  widget_host->SetCursor(custom_cursor);
  EXPECT_EQ(content::CursorUtils::GetLastCursorForWebContents(
                test_api_->manager()->GetAssociatedWebContents()),
            ui::mojom::CursorType::kPointer);

  // After the prompt is resolved, custom cursors are allowed again.
  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  widget_host->SetCursor(custom_cursor);
  EXPECT_EQ(content::CursorUtils::GetLastCursorForWebContents(
                test_api_->manager()->GetAssociatedWebContents()),
            ui::mojom::CursorType::kCustom);
}

class PermissionPromptBubbleViewQuietUiBrowserTest
    : public PermissionPromptBubbleViewBrowserTest {
 public:
  PermissionPromptBubbleViewQuietUiBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kQuietNotificationPrompts},
        {permissions::features::kPermissionQuietChip});
  }

 protected:
  using QuietUiReason = permissions::PermissionUiSelector::QuietUiReason;
  using WarningReason = permissions::PermissionUiSelector::WarningReason;

  void SetCannedUiDecision(absl::optional<QuietUiReason> quiet_ui_reason,
                           absl::optional<WarningReason> warning_reason) {
    test_api_->manager()->set_permission_ui_selector_for_testing(
        std::make_unique<TestQuietNotificationPermissionUiSelector>(
            permissions::PermissionUiSelector::Decision(quiet_ui_reason,
                                                        warning_reason)));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that the quiet prompt disposition differs when permission is considered
// abusive (currently only applicable for Notifications) vs. when permission is
// not considered abusive. For `QuietUiReason::kTriggeredDueToAbusiveContent`
// reputation we show a static UI icon.
IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewQuietUiBrowserTest,
                       DispositionAbusiveContentTest) {
  SetCannedUiDecision(QuietUiReason::kTriggeredDueToAbusiveContent,
                      WarningReason::kAbusiveContent);

  base::HistogramTester histograms;

  ShowUi("geolocation");

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      GetParam() ? permissions::PermissionPromptDisposition::
                       LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE
                 : permissions::PermissionPromptDisposition::ANCHORED_BUBBLE);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  histograms.ExpectBucketCount(
      GetParam() ? "Permissions.Prompt.Geolocation."
                   "LocationBarLeftChipAutoBubble.Action"
                 : "Permissions.Prompt.Geolocation.AnchoredBubble.Action",
      static_cast<int>(permissions::PermissionAction::GRANTED), 1);

  ShowUi("notifications");

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::LOCATION_BAR_RIGHT_STATIC_ICON);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  histograms.ExpectBucketCount(
      "Permissions.Prompt.Notifications.LocationBarRightStaticIcon.Action",
      static_cast<int>(permissions::PermissionAction::GRANTED), 1);
}

IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewQuietUiBrowserTest,
                       DispositionCrowdDenyTest) {
  SetCannedUiDecision(QuietUiReason::kTriggeredByCrowdDeny, absl::nullopt);

  ShowUi("geolocation");

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      GetParam() ? permissions::PermissionPromptDisposition::
                       LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE
                 : permissions::PermissionPromptDisposition::ANCHORED_BUBBLE);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  ShowUi("notifications");

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::LOCATION_BAR_RIGHT_STATIC_ICON);
}

// For `QuietUiReason::kEnabledInPrefs` reputation we show an animated quiet UI
// icon.
IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewQuietUiBrowserTest,
                       DispositionEnabledInPrefsTest) {
  SetCannedUiDecision(QuietUiReason::kEnabledInPrefs, absl::nullopt);

  base::HistogramTester histograms;

  ShowUi("geolocation");

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      GetParam() ? permissions::PermissionPromptDisposition::
                       LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE
                 : permissions::PermissionPromptDisposition::ANCHORED_BUBBLE);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  histograms.ExpectBucketCount(
      GetParam() ? "Permissions.Prompt.Geolocation."
                   "LocationBarLeftChipAutoBubble.Action"
                 : "Permissions.Prompt.Geolocation.AnchoredBubble.Action",
      static_cast<int>(permissions::PermissionAction::GRANTED), 1);

  ShowUi("notifications");

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_RIGHT_ANIMATED_ICON);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  histograms.ExpectBucketCount(
      "Permissions.Prompt.Notifications.LocationBarRightAnimatedIcon.Action",
      static_cast<int>(permissions::PermissionAction::GRANTED), 1);
}

// For `QuietUiReason::kServicePredictedVeryUnlikelyGrant` reputation we show an
// animated quiet UI icon.
IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewQuietUiBrowserTest,
                       DispositionPredictedVeryUnlikelyGrantTest) {
  SetCannedUiDecision(QuietUiReason::kServicePredictedVeryUnlikelyGrant,
                      absl::nullopt);

  ShowUi("geolocation");

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      GetParam() ? permissions::PermissionPromptDisposition::
                       LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE
                 : permissions::PermissionPromptDisposition::ANCHORED_BUBBLE);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  ShowUi("notifications");

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_RIGHT_ANIMATED_ICON);
}

// For `QuietUiReason::kTriggeredDueToAbusiveRequests` reputation we show a
// static quiet UI icon.
IN_PROC_BROWSER_TEST_P(PermissionPromptBubbleViewQuietUiBrowserTest,
                       DispositionAbusiveRequestsTest) {
  SetCannedUiDecision(QuietUiReason::kTriggeredDueToAbusiveRequests,
                      WarningReason::kAbusiveRequests);

  ShowUi("geolocation");

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      GetParam() ? permissions::PermissionPromptDisposition::
                       LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE
                 : permissions::PermissionPromptDisposition::ANCHORED_BUBBLE);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  ShowUi("notifications");

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::LOCATION_BAR_RIGHT_STATIC_ICON);
}

class QuietChipPermissionPromptBubbleViewBrowserTest
    : public PermissionPromptBubbleViewQuietUiBrowserTest {
 public:
  QuietChipPermissionPromptBubbleViewBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        permissions::features::kPermissionQuietChip);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(QuietChipPermissionPromptBubbleViewBrowserTest,
                       LoudChipOrAnchoredBubbleIsShownForNonAbusiveRequests) {
  SetCannedUiDecision(absl::nullopt, absl::nullopt);

  ShowUi("geolocation");

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      GetParam() ? permissions::PermissionPromptDisposition::
                       LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE
                 : permissions::PermissionPromptDisposition::ANCHORED_BUBBLE);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  ShowUi("notifications");

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      GetParam() ? permissions::PermissionPromptDisposition::
                       LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE
                 : permissions::PermissionPromptDisposition::ANCHORED_BUBBLE);
}

IN_PROC_BROWSER_TEST_P(QuietChipPermissionPromptBubbleViewBrowserTest,
                       QuietChipIsShownForAbusiveRequests) {
  for (QuietUiReason reason : {QuietUiReason::kTriggeredByCrowdDeny,
                               QuietUiReason::kTriggeredDueToAbusiveRequests,
                               QuietUiReason::kTriggeredDueToAbusiveContent}) {
    SetCannedUiDecision(reason, absl::nullopt);

    ShowUi("geolocation");

    EXPECT_EQ(
        test_api_->manager()->current_request_prompt_disposition_for_testing(),
        GetParam() ? permissions::PermissionPromptDisposition::
                         LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE
                   : permissions::PermissionPromptDisposition::ANCHORED_BUBBLE);

    test_api_->manager()->Accept();
    base::RunLoop().RunUntilIdle();

    ShowUi("notifications");

    // Quiet Chip is enabled, that means a quiet chip will be shown even if the
    // Chip experiment is disabled.
    EXPECT_EQ(
        test_api_->manager()->current_request_prompt_disposition_for_testing(),
        permissions::PermissionPromptDisposition::
            LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP);
  }
}

class OneTimePermissionPromptBubbleViewBrowserTest
    : public PermissionPromptBubbleViewBrowserTest {
 public:
  OneTimePermissionPromptBubbleViewBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        permissions::features::kOneTimePermission,
        {{"OkButtonBehavesAsAllowAlways", GetParam() ? "true" : "false"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(OneTimePermissionPromptBubbleViewBrowserTest,
                       InvokeUi_geolocation) {
  ShowAndVerifyUi();
}

// False / True values determine if the PermissionChip feature is
// disabled/enabled.
INSTANTIATE_TEST_SUITE_P(All,
                         PermissionPromptBubbleViewBrowserTest,
                         ::testing::Values(false, true));
INSTANTIATE_TEST_SUITE_P(All,
                         PermissionPromptBubbleViewQuietUiBrowserTest,
                         ::testing::Values(false, true));
INSTANTIATE_TEST_SUITE_P(All,
                         QuietChipPermissionPromptBubbleViewBrowserTest,
                         ::testing::Values(false, true));
INSTANTIATE_TEST_SUITE_P(All,
                         OneTimePermissionPromptBubbleViewBrowserTest,
                         ::testing::Values(false, true));
INSTANTIATE_TEST_SUITE_P(All, QuietUIPromoBrowserTest, ::testing::Values(true));
