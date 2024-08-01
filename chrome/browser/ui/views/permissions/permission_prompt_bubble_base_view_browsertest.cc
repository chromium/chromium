// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"

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
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/permissions/chip/chip_controller.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_chip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/custom_handlers/register_protocol_handler_permission_request.h"
#include "components/permissions/constants.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_ui_selector.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_request.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/cursor_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/button_test_api.h"
#include "url/gurl.h"

// To run the pixel tests of this file run: browser_tests
// --gtest_filter=BrowserUiTest.Invoke --test-launcher-interactive
// --enable-pixel-output-in-tests --ui=<test name e.g.
// PermissionPromptBubbleBaseViewBrowserTest>.*
//
// Check go/brapp-desktop-pixel-tests for more info.

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

class PermissionPromptBubbleBaseViewBrowserTest : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("a.com", "/empty.html")));

    test_api_ =
        std::make_unique<test::PermissionRequestManagerTestApi>(browser());
  }

 protected:
  using QuietUiReason = permissions::PermissionUiSelector::QuietUiReason;
  using WarningReason = permissions::PermissionUiSelector::WarningReason;

  void SetCannedUiDecision(std::optional<QuietUiReason> quiet_ui_reason,
                           std::optional<WarningReason> warning_reason) {
    GetTestApi().manager()->set_permission_ui_selector_for_testing(
        std::make_unique<TestQuietNotificationPermissionUiSelector>(
            permissions::PermissionUiSelector::Decision(quiet_ui_reason,
                                                        warning_reason)));
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    const std::string& actual_name = name.substr(0, name.find("/"));
    AddRequestForContentSetting(actual_name);
    base::RunLoop().RunUntilIdle();
  }

  GURL GetTestUrl() { return test_url_; }
  void SetTestUrl(GURL test_url) { test_url_ = test_url; }

  content::RenderFrameHost* GetActiveMainFrame() {
    return browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetPrimaryMainFrame();
  }

  ChipController* GetChipController() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->toolbar()->location_bar()->GetChipController();
  }

  ContentSettingImageView& GetContentSettingImageView(
      ContentSettingImageModel::ImageType image_type) {
    LocationBarView* location_bar_view =
        BrowserView::GetBrowserViewForBrowser(browser())->GetLocationBarView();
    return **base::ranges::find(
        location_bar_view->GetContentSettingViewsForTest(), image_type,
        &ContentSettingImageView::GetType);
  }

  test::PermissionRequestManagerTestApi& GetTestApi() { return *test_api_; }
  void SetEmbeddingOrigin(const GURL& origin) { embedding_origin_ = origin; }

 private:
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
      if (name == it->name) {
        break;
      }
    }
    if (it == std::end(kNameToType)) {
      ADD_FAILURE() << "Unknown: " << name;
      return;
    }
    permissions::PermissionRequestManager* manager = test_api_->manager();
    content::RenderFrameHost* source_frame = GetActiveMainFrame();

    // Pixel verification for storage_access test checks a permission
    // request prompt that has an origin and port. Because these tests run
    // on localhost, the port changes, and the test pixel verification
    // fails. We need a fixed URL, so the Gold image used in the pixel test
    // always matches with the output of the test.
    if (it->type == ContentSettingsType::STORAGE_ACCESS) {
      test_api_->manager()->set_embedding_origin_for_testing(embedding_origin_);
    }

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

  // Disable chip animations so permission chips and prompts appear immediately.
  const gfx::AnimationTestApi::RenderModeResetter disable_rich_animations_ =
      gfx::AnimationTestApi::SetRichAnimationRenderMode(
          gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<test::PermissionRequestManagerTestApi> test_api_;
  GURL embedding_origin_ = GURL("https://www.origin.test.com");
  GURL test_url_ = GURL("https://example.com");
};

// Flaky on Mac: http://crbug.com/1502621
#if BUILDFLAG(IS_MAC)
#define MAYBE_AlertAccessibleEvent DISABLED_AlertAccessibleEvent
#else
#define MAYBE_AlertAccessibleEvent AlertAccessibleEvent
#endif
IN_PROC_BROWSER_TEST_F(PermissionPromptBubbleBaseViewBrowserTest,
                       MAYBE_AlertAccessibleEvent) {
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

// Test switching between PermissionChip and PermissionPromptBubbleBaseView and
// make sure no crashes.
IN_PROC_BROWSER_TEST_F(PermissionPromptBubbleBaseViewBrowserTest,
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

// crbug.com/989858
#if BUILDFLAG(IS_WIN)
#define MAYBE_ActiveTabClosedAfterRendererCrashesWithPendingPermissionRequest \
  DISABLED_ActiveTabClosedAfterRendererCrashesWithPendingPermissionRequest
#else
#define MAYBE_ActiveTabClosedAfterRendererCrashesWithPendingPermissionRequest \
  ActiveTabClosedAfterRendererCrashesWithPendingPermissionRequest
#endif
// Regression test for https://crbug.com/933321.
IN_PROC_BROWSER_TEST_F(
    PermissionPromptBubbleBaseViewBrowserTest,
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
IN_PROC_BROWSER_TEST_F(PermissionPromptBubbleBaseViewBrowserTest,
                       InvokeUi_geolocation) {
  ShowAndVerifyUi();
}

// Host wants to show notifications.
IN_PROC_BROWSER_TEST_F(PermissionPromptBubbleBaseViewBrowserTest,
                       InvokeUi_notifications) {
  ShowAndVerifyUi();
}

// Host wants to use your microphone.
IN_PROC_BROWSER_TEST_F(PermissionPromptBubbleBaseViewBrowserTest,
                       InvokeUi_mic) {
  ShowAndVerifyUi();
}

// Host wants to use your camera.
IN_PROC_BROWSER_TEST_F(PermissionPromptBubbleBaseViewBrowserTest,
                       InvokeUi_camera) {
  ShowAndVerifyUi();
}

// Host wants to open email links.
IN_PROC_BROWSER_TEST_F(PermissionPromptBubbleBaseViewBrowserTest,
                       InvokeUi_protocol_handlers) {
  ShowAndVerifyUi();
}

// Host wants to use your MIDI devices.
IN_PROC_BROWSER_TEST_F(PermissionPromptBubbleBaseViewBrowserTest,
                       InvokeUi_midi) {
  ShowAndVerifyUi();
}

// Host wants to access storage from the site in which it's embedded.
IN_PROC_BROWSER_TEST_F(PermissionPromptBubbleBaseViewBrowserTest,
                       InvokeUi_storage_access) {
  ShowAndVerifyUi();
}

// Host wants to trigger multiple downloads.
IN_PROC_BROWSER_TEST_F(PermissionPromptBubbleBaseViewBrowserTest,
                       InvokeUi_downloads) {
  ShowAndVerifyUi();
}

// Shows a permissions bubble with multiple requests.
IN_PROC_BROWSER_TEST_F(PermissionPromptBubbleBaseViewBrowserTest,
                       InvokeUi_multiple) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PermissionPromptBubbleBaseViewBrowserTest,
                       OpenHelpCenterLinkInNewTab) {
  ShowUi("storage_access");

  // Get link widget from the prompt.
  views::Widget* prompt = GetTestApi().GetPromptWindow();
  ASSERT_TRUE(prompt);
  auto* label_with_link =
      static_cast<views::StyledLabel*>(prompt->GetRootView()->GetViewByID(
          permissions::PermissionPromptViewID::VIEW_ID_PERMISSION_PROMPT_LINK));
  ASSERT_TRUE(label_with_link);

  // Click on the help center link and check that it opens on a new tab.
  content::WebContentsAddedObserver new_tab_observer;
  label_with_link->ClickFirstLinkForTesting();
  GURL url = GURL(permissions::kEmbeddedContentHelpCenterURL);

  EXPECT_EQ(new_tab_observer.GetWebContents()->GetVisibleURL(), url);
}

// ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER is ChromeOS only.
#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PermissionPromptBubbleBaseViewBrowserTest,
                       InvokeUi_protected_media) {
  ShowAndVerifyUi();
}
#endif

// Test that the quiet prompt disposition returns the same value when permission
// is not considered abusive (currently only applicable for Notifications) vs.
// when permission is not considered abusive.
IN_PROC_BROWSER_TEST_F(PermissionPromptBubbleBaseViewBrowserTest,
                       DispositionNoAbusiveTest) {
  base::HistogramTester histograms;

  ShowUi("geolocation");

  EXPECT_EQ(
      GetTestApi().manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);

  base::TimeDelta duration = base::Milliseconds(42);
  GetTestApi().manager()->set_time_to_decision_for_test(duration);

  GetTestApi().manager()->Accept();
  base::RunLoop().RunUntilIdle();

    histograms.ExpectBucketCount(
        "Permissions.Prompt.Geolocation.LocationBarLeftChipAutoBubble.Action",
        static_cast<int>(permissions::PermissionAction::GRANTED), 1);
    histograms.ExpectTimeBucketCount(
        "Permissions.Prompt.Geolocation.LocationBarLeftChipAutoBubble.Accepted."
        "TimeToAction",
        duration, 1);

  ShowUi("notifications");

  EXPECT_EQ(
      GetTestApi().manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);

  duration = base::Milliseconds(42);
  GetTestApi().manager()->set_time_to_decision_for_test(duration);

  GetTestApi().manager()->Accept();
  base::RunLoop().RunUntilIdle();

    histograms.ExpectBucketCount(
        "Permissions.Prompt.Notifications.LocationBarLeftChipAutoBubble.Action",
        static_cast<int>(permissions::PermissionAction::GRANTED), 1);
    histograms.ExpectTimeBucketCount(
        "Permissions.Prompt.Notifications.LocationBarLeftChipAutoBubble."
        "Accepted."
        "TimeToAction",
        duration, 1);
}

IN_PROC_BROWSER_TEST_F(PermissionPromptBubbleBaseViewBrowserTest,
                       AcceptedOnceDispositionNoAbusiveTest) {
  base::HistogramTester histograms;

  ShowUi("geolocation");

  EXPECT_EQ(
      GetTestApi().manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);

  base::TimeDelta duration = base::Milliseconds(42);
  GetTestApi().manager()->set_time_to_decision_for_test(duration);

  GetTestApi().manager()->AcceptThisTime();
  base::RunLoop().RunUntilIdle();

    histograms.ExpectBucketCount(
        "Permissions.Prompt.Geolocation.LocationBarLeftChipAutoBubble.Action",
        static_cast<int>(permissions::PermissionAction::GRANTED_ONCE), 1);
    histograms.ExpectTimeBucketCount(
        "Permissions.Prompt.Geolocation.LocationBarLeftChipAutoBubble."
        "AcceptedOnce."
        "TimeToAction",
        duration, 1);

  ShowUi("notifications");

  EXPECT_EQ(
      GetTestApi().manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);

  duration = base::Milliseconds(42);
  GetTestApi().manager()->set_time_to_decision_for_test(duration);

  GetTestApi().manager()->AcceptThisTime();
  base::RunLoop().RunUntilIdle();

    histograms.ExpectBucketCount(
        "Permissions.Prompt.Notifications.LocationBarLeftChipAutoBubble.Action",
        static_cast<int>(permissions::PermissionAction::GRANTED_ONCE), 1);
    histograms.ExpectTimeBucketCount(
        "Permissions.Prompt.Notifications.LocationBarLeftChipAutoBubble."
        "AcceptedOnce."
        "TimeToAction",
        duration, 1);
}

IN_PROC_BROWSER_TEST_F(PermissionPromptBubbleBaseViewBrowserTest,
                       PermissionPromptBubbleDisallowsCustomCursors) {
  ui::Cursor custom_cursor(ui::mojom::CursorType::kCustom);

  content::RenderWidgetHost* widget_host = GetTestApi()
                                               .manager()
                                               ->GetAssociatedWebContents()
                                               ->GetRenderViewHost()
                                               ->GetWidget();

  // Initially custom cursors are allowed.
  widget_host->SetCursor(custom_cursor);
  EXPECT_EQ(content::CursorUtils::GetLastCursorForWebContents(
                GetTestApi().manager()->GetAssociatedWebContents()),
            ui::mojom::CursorType::kCustom);

  // While a permission prompt is active custom cursors are not allowed.
  ShowUi("geolocation");
  EXPECT_EQ(
      GetTestApi().manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);

  widget_host->SetCursor(custom_cursor);
  EXPECT_EQ(content::CursorUtils::GetLastCursorForWebContents(
                GetTestApi().manager()->GetAssociatedWebContents()),
            ui::mojom::CursorType::kPointer);

  // After the prompt is resolved, custom cursors are allowed again.
  GetTestApi().manager()->Accept();
  base::RunLoop().RunUntilIdle();

  widget_host->SetCursor(custom_cursor);
  EXPECT_EQ(content::CursorUtils::GetLastCursorForWebContents(
                GetTestApi().manager()->GetAssociatedWebContents()),
            ui::mojom::CursorType::kCustom);
}

IN_PROC_BROWSER_TEST_F(PermissionPromptBubbleBaseViewBrowserTest,
                       LoudChipOrAnchoredBubbleIsShownForNonAbusiveRequests) {
  SetCannedUiDecision(std::nullopt, std::nullopt);

  ShowUi("geolocation");

  EXPECT_EQ(
      GetTestApi().manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);

  GetTestApi().manager()->Accept();
  base::RunLoop().RunUntilIdle();

  ShowUi("notifications");

  EXPECT_EQ(
      GetTestApi().manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);
}

IN_PROC_BROWSER_TEST_F(PermissionPromptBubbleBaseViewBrowserTest,
                       QuietChipIsShownForAbusiveRequests) {
  for (QuietUiReason reason : {QuietUiReason::kTriggeredByCrowdDeny,
                               QuietUiReason::kTriggeredDueToAbusiveRequests,
                               QuietUiReason::kTriggeredDueToAbusiveContent}) {
    SetCannedUiDecision(reason, std::nullopt);

    ShowUi("geolocation");

    EXPECT_EQ(GetTestApi()
                  .manager()
                  ->current_request_prompt_disposition_for_testing(),
              permissions::PermissionPromptDisposition::
                  LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);

    GetTestApi().manager()->Accept();
    base::RunLoop().RunUntilIdle();

    ShowUi("notifications");

    // Quiet Chip is enabled, that means a quiet chip will be shown even if the
    // Chip experiment is disabled.
    EXPECT_EQ(GetTestApi()
                  .manager()
                  ->current_request_prompt_disposition_for_testing(),
              permissions::PermissionPromptDisposition::
                  LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP);
  }
}

class PermissionPromptBubbleBaseViewAllowAlwaysFirstBrowserTest
    : public PermissionPromptBubbleBaseViewBrowserTest {
 public:
  PermissionPromptBubbleBaseViewAllowAlwaysFirstBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        permissions::features::kOneTimePermission,
        {{"show_allow_always_as_first_button", "true"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    PermissionPromptBubbleBaseViewAllowAlwaysFirstBrowserTest,
    InvokeUi_geolocation) {
  ShowAndVerifyUi();
}

class PermissionPromptBubbleBaseViewAllowWhileVisitingFirstBrowserTest
    : public PermissionPromptBubbleBaseViewBrowserTest {
 public:
  PermissionPromptBubbleBaseViewAllowWhileVisitingFirstBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        permissions::features::kOneTimePermission,
        {{"use_stronger_prompt_language", "true"},
         {"use_while_visiting_language", "true"},
         {"show_allow_always_as_first_button", "true"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    PermissionPromptBubbleBaseViewAllowWhileVisitingFirstBrowserTest,
    InvokeUi_geolocation) {
  ShowAndVerifyUi();
}

class LongOriginPermissionPromptBubbleBaseViewBrowserTest
    : public PermissionPromptBubbleBaseViewBrowserTest {
 public:
  void SetUpOnMainThread() override {
    PermissionPromptBubbleBaseViewBrowserTest::SetUpOnMainThread();
    SetOrigin(long_origin_);
    SetTestUrl(long_origin_);
  }

  void SetOrigin(const GURL& origin) { GetTestApi().SetOrigin(origin); }

  GURL long_origin_ = GURL(
      "https://"
      "example.example.example.example.example.example.example.example."
      "example.example.example.example.example.example.example.example."
      "example.example.example.example.example.example.example.example."
      "example.example.example.example.example.example.example.example."
      "example.example.example.examp");
};

IN_PROC_BROWSER_TEST_F(LongOriginPermissionPromptBubbleBaseViewBrowserTest,
                       InvokeUi_geolocation) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(LongOriginPermissionPromptBubbleBaseViewBrowserTest,
                       InvokeUi_notifications) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(LongOriginPermissionPromptBubbleBaseViewBrowserTest,
                       InvokeUi_mic) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(LongOriginPermissionPromptBubbleBaseViewBrowserTest,
                       InvokeUi_camera) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(LongOriginPermissionPromptBubbleBaseViewBrowserTest,
                       InvokeUi_protocol_handlers) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(LongOriginPermissionPromptBubbleBaseViewBrowserTest,
                       InvokeUi_midi) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(LongOriginPermissionPromptBubbleBaseViewBrowserTest,
                       InvokeUi_storage_access) {
  SetEmbeddingOrigin(long_origin_);
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(LongOriginPermissionPromptBubbleBaseViewBrowserTest,
                       InvokeUi_downloads) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(LongOriginPermissionPromptBubbleBaseViewBrowserTest,
                       InvokeUi_multiple) {
  ShowAndVerifyUi();
}
