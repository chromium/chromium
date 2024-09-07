// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/browser/ui/views/page_info/permission_toggle_row_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/events/test/test_event.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/interaction/interaction_test_util_views.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
const char kFirstPermissionRow[] = "FirstPermissionRow";

// Clicks the location icon to open the page info bubble.
void OpenPageInfoBubble(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  LocationIconView* location_icon_view =
      browser_view->toolbar()->location_bar()->location_icon_view();
  ASSERT_TRUE(location_icon_view);
  ui::test::TestEvent event;
  location_icon_view->ShowBubble(event);
  views::BubbleDialogDelegateView* page_info =
      PageInfoBubbleView::GetPageInfoBubbleForTesting();
  EXPECT_NE(nullptr, page_info);
  page_info->set_close_on_deactivate(false);
}

// Tracks focus of an arbitrary UI element.
class FocusTracker {
 public:
  FocusTracker(const FocusTracker&) = delete;
  FocusTracker& operator=(const FocusTracker&) = delete;

  bool focused() const { return focused_; }

  // Wait for focused() to be in state |target_state_is_focused|. If focused()
  // is already in the desired state, returns immediately, otherwise waits until
  // it is.
  void WaitForFocus(bool target_state_is_focused) {
    if (focused_ == target_state_is_focused)
      return;
    target_state_is_focused_ = target_state_is_focused;
    run_loop_.Run();
  }

 protected:
  explicit FocusTracker(bool initially_focused) : focused_(initially_focused) {}
  virtual ~FocusTracker() = default;

  void OnFocused() {
    focused_ = true;
    if (run_loop_.running() && target_state_is_focused_ == focused_)
      run_loop_.Quit();
  }

  void OnBlurred() {
    focused_ = false;
    if (run_loop_.running() && target_state_is_focused_ == focused_)
      run_loop_.Quit();
  }

 private:
  // Whether the tracked visual element is currently focused.
  bool focused_ = false;

  // Desired state when waiting for focus to change.
  bool target_state_is_focused_;

  base::RunLoop run_loop_;
};

// Watches a WebContents for focus changes.
class WebContentsFocusTracker : public FocusTracker,
                                public content::WebContentsObserver {
 public:
  explicit WebContentsFocusTracker(content::WebContents* web_contents)
      : FocusTracker(IsWebContentsFocused(web_contents)),
        WebContentsObserver(web_contents) {}

  void OnWebContentsFocused(
      content::RenderWidgetHost* render_widget_host) override {
    OnFocused();
  }

  void OnWebContentsLostFocus(
      content::RenderWidgetHost* render_widget_host) override {
    OnBlurred();
  }

 private:
  static bool IsWebContentsFocused(content::WebContents* web_contents) {
    Browser* const browser = chrome::FindBrowserWithTab(web_contents);
    if (!browser)
      return false;
    if (browser->tab_strip_model()->GetActiveWebContents() != web_contents)
      return false;
    return BrowserView::GetBrowserViewForBrowser(browser)
        ->contents_web_view()
        ->HasFocus();
  }
};

// Watches a View for focus changes.
class ViewFocusTracker : public FocusTracker, public views::ViewObserver {
 public:
  explicit ViewFocusTracker(views::View* view)
      : FocusTracker(view->HasFocus()) {
    scoped_observation_.Observe(view);
  }

  void OnViewFocused(views::View* observed_view) override { OnFocused(); }

  void OnViewBlurred(views::View* observed_view) override { OnBlurred(); }

 private:
  base::ScopedObservation<views::View, views::ViewObserver> scoped_observation_{
      this};
};

}  // namespace

class PageInfoBubbleViewFocusInteractiveUiTest : public InProcessBrowserTest {
 public:
  PageInfoBubbleViewFocusInteractiveUiTest() = default;
  PageInfoBubbleViewFocusInteractiveUiTest(
      const PageInfoBubbleViewFocusInteractiveUiTest& test) = delete;
  PageInfoBubbleViewFocusInteractiveUiTest& operator=(
      const PageInfoBubbleViewFocusInteractiveUiTest& test) = delete;

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void TriggerReloadPromptOnClose() const {
    // Set some dummy non-default permissions. This will trigger a reload prompt
    // when the bubble is closed.
    PageInfo::PermissionInfo permission;
    permission.type = ContentSettingsType::NOTIFICATIONS;
    permission.setting = ContentSetting::CONTENT_SETTING_BLOCK;
    permission.default_setting = ContentSetting::CONTENT_SETTING_ASK;
    permission.source = content_settings::SettingSource::kUser;

    PageInfo* presenter = static_cast<PageInfoBubbleView*>(
                              PageInfoBubbleView::GetPageInfoBubbleForTesting())
                              ->presenter_for_testing();
    presenter->OnSitePermissionChanged(permission.type, permission.setting,
                                       permission.requesting_origin,
                                       permission.is_one_time);
  }
};

#if BUILDFLAG(IS_MAC)
// https://crbug.com/1029882
#define MAYBE_FocusReturnsToContentOnClose DISABLED_FocusReturnsToContentOnClose
#else
#define MAYBE_FocusReturnsToContentOnClose FocusReturnsToContentOnClose
#endif

// Test that when the PageInfo bubble is closed, focus is returned to the web
// contents pane.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewFocusInteractiveUiTest,
                       MAYBE_FocusReturnsToContentOnClose) {
  WebContentsFocusTracker web_contents_focus_tracker(web_contents());
  web_contents()->Focus();
  web_contents_focus_tracker.WaitForFocus(true);

  OpenPageInfoBubble(browser());
  base::RunLoop().RunUntilIdle();

  auto* page_info_bubble_view =
      PageInfoBubbleView::GetPageInfoBubbleForTesting();
  EXPECT_FALSE(web_contents_focus_tracker.focused());

  page_info_bubble_view->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kEscKeyPressed);
  web_contents_focus_tracker.WaitForFocus(true);
  EXPECT_TRUE(web_contents_focus_tracker.focused());
}

#if BUILDFLAG(IS_MAC)
// https://crbug.com/1029882
#define MAYBE_FocusDoesNotReturnToContentsOnReloadPrompt \
  DISABLED_FocusDoesNotReturnToContentsOnReloadPrompt
#else
#define MAYBE_FocusDoesNotReturnToContentsOnReloadPrompt \
  FocusDoesNotReturnToContentsOnReloadPrompt
#endif

// Test that when the PageInfo bubble is closed and a reload prompt is
// displayed, focus is NOT returned to the web contents pane, but rather returns
// to the location bar so accessibility users must tab through the reload prompt
// before getting back to web contents (see https://crbug.com/910067).
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewFocusInteractiveUiTest,
                       MAYBE_FocusDoesNotReturnToContentsOnReloadPrompt) {
  WebContentsFocusTracker web_contents_focus_tracker(web_contents());
  ViewFocusTracker location_bar_focus_tracker(
      BrowserView::GetBrowserViewForBrowser(browser())->GetLocationBarView());
  web_contents()->Focus();
  web_contents_focus_tracker.WaitForFocus(true);

  OpenPageInfoBubble(browser());
  base::RunLoop().RunUntilIdle();

  auto* page_info_bubble_view =
      PageInfoBubbleView::GetPageInfoBubbleForTesting();
  EXPECT_FALSE(web_contents_focus_tracker.focused());

  TriggerReloadPromptOnClose();
  page_info_bubble_view->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kEscKeyPressed);
  location_bar_focus_tracker.WaitForFocus(true);
  web_contents_focus_tracker.WaitForFocus(false);
  EXPECT_TRUE(location_bar_focus_tracker.focused());
  EXPECT_FALSE(web_contents_focus_tracker.focused());
}

class PageInfoBubbleViewInteractiveUiTest : public InteractiveBrowserTest {
 public:
  PageInfoBubbleViewInteractiveUiTest() = default;
  ~PageInfoBubbleViewInteractiveUiTest() override = default;
  PageInfoBubbleViewInteractiveUiTest(
      const PageInfoBubbleViewInteractiveUiTest&) = delete;
  void operator=(const PageInfoBubbleViewInteractiveUiTest&) = delete;

  void SetUp() override {
    content_settings::ContentSettingsRegistry::GetInstance()->ResetForTest();
    content_settings::ContentSettingsRegistry::GetInstance();
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);

    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server()->ServeFilesFromSourceDirectory(GetChromeTestDataDir());

    ASSERT_TRUE(https_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(https_server());
    https_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  ui::ElementContext context() const {
    return browser()->window()->GetElementContext();
  }

  // Navigates a tab to `GetURL()` and opens PageInfo.
  auto NavigateAndOpenPageInfo() {
    return Steps(InstrumentTab(kWebContentsElementId),
                 NavigateWebContents(kWebContentsElementId, GetURL()),
                 PressButton(kLocationIconElementId));
  }

  auto TogglePermission(ElementSpecifier row) {
    return WithElement(
        row, base::BindOnce([](ui::TrackedElement* el) {
          views::test::InteractionTestUtilSimulatorViews::PressButton(
              static_cast<views::Button*>(AsView<PermissionToggleRowView>(el)
                                              ->toggle_button_for_testing()));
        }));
  }

  auto CheckContentSettings(ContentSettingsType type, ContentSetting setting) {
    return CheckResult(
        base::BindLambdaForTesting([type, this]() {
          return host_content_settings_map()->GetContentSetting(GetURL(),
                                                                GetURL(), type);
        }),
        setting,
        "Checking if the content setting value matches the expectation");
  }

 protected:
  GURL GetURL() {
    return https_server()->GetURL("a.test", "/permissions/requests.html");
  }

  HostContentSettingsMap* host_content_settings_map() {
    return HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  }

  void SetPermission(ContentSettingsType type, ContentSetting setting) {
    host_content_settings_map()->SetContentSettingDefaultScope(
        GetURL(), GetURL(), type, setting);
  }

  void SetBroadException(ContentSettingsType type, ContentSetting setting) {
    host_content_settings_map()->SetContentSettingCustomScope(
        ContentSettingsPattern::FromString("https://*"),
        ContentSettingsPattern::Wildcard(), type, setting);
  }

  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewInteractiveUiTest,
                       ToggleTest_DefaultAsk) {
  ASSERT_EQ(host_content_settings_map()->GetDefaultContentSetting(
                ContentSettingsType::NOTIFICATIONS),
            CONTENT_SETTING_ASK);
  // Set Notifications permission to Allow so it becomes visible in PageInfo.
  SetPermission(ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW);

  RunTestSequenceInContext(
      context(), NavigateAndOpenPageInfo(),
      CheckViewProperty(PageInfoMainView::kMainLayoutElementId,
                        &PageInfoMainView::GetVisiblePermissionsCountForTesting,
                        1),
      // A view with permissions in PageInfo
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      // Set id to the first children of `kPermissionsElementId` -
      // permissions view in PageInfo.
      NameChildView(PageInfoMainView::kPermissionsElementId,
                    kFirstPermissionRow, 0u),
      // Verify the row label is Notifications
      CheckViewProperty(
          kFirstPermissionRow, &PermissionToggleRowView::GetRowTitleForTesting,
          l10n_util::GetStringUTF16(IDS_SITE_SETTINGS_TYPE_NOTIFICATIONS)),
      // Verify the toggle is on.
      CheckViewProperty(
          kFirstPermissionRow,
          &PermissionToggleRowView::GetToggleButtonStateForTesting, true),
      // Verify the content setting is Allow.
      CheckContentSettings(ContentSettingsType::NOTIFICATIONS,
                           CONTENT_SETTING_ALLOW),
      // Toggle the button to block.
      TogglePermission(kFirstPermissionRow),
      // Verify the toggle is off.
      CheckViewProperty(
          kFirstPermissionRow,
          &PermissionToggleRowView::GetToggleButtonStateForTesting, false),
      // Verify the content setting is Block.
      CheckContentSettings(ContentSettingsType::NOTIFICATIONS,
                           CONTENT_SETTING_BLOCK),
      // Return to allow.
      TogglePermission(kFirstPermissionRow),
      // Verify the toggle is back on.
      CheckViewProperty(
          kFirstPermissionRow,
          &PermissionToggleRowView::GetToggleButtonStateForTesting, true),
      // Verify the content setting is Allow.
      CheckContentSettings(ContentSettingsType::NOTIFICATIONS,
                           CONTENT_SETTING_ALLOW));
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewInteractiveUiTest,
                       ToggleTest_DefaultBlock) {
  // Set Notifications permission to blocked by default.
  host_content_settings_map()->SetDefaultContentSetting(
      ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_BLOCK);

  RunTestSequenceInContext(
      context(), NavigateAndOpenPageInfo(),
      CheckViewProperty(PageInfoMainView::kMainLayoutElementId,
                        &PageInfoMainView::GetVisiblePermissionsCountForTesting,
                        1),
      // A view with permissions in PageInfo
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      // Set id to the first children of `kPermissionsElementId` -
      // permissions view in PageInfo.
      NameChildView(PageInfoMainView::kPermissionsElementId,
                    kFirstPermissionRow, 0u),
      // Verify the row label is Notifications
      CheckViewProperty(
          kFirstPermissionRow, &PermissionToggleRowView::GetRowTitleForTesting,
          l10n_util::GetStringUTF16(IDS_SITE_SETTINGS_TYPE_NOTIFICATIONS)),
      // Verify the toggle is off.
      CheckViewProperty(
          kFirstPermissionRow,
          &PermissionToggleRowView::GetToggleButtonStateForTesting, false),
      // Verify the content setting is Block.
      CheckContentSettings(ContentSettingsType::NOTIFICATIONS,
                           CONTENT_SETTING_BLOCK),
      // Verify that the subtitle for the block (default) state.
      CheckViewProperty(kFirstPermissionRow,
                        &PermissionToggleRowView::GetRowSubTitleForTesting,
                        u"Not allowed (default)"),
      // Toggle the button to block.
      TogglePermission(kFirstPermissionRow),
      // Verify the toggle is on.
      CheckViewProperty(
          kFirstPermissionRow,
          &PermissionToggleRowView::GetToggleButtonStateForTesting, true),
      // Verify the content setting is Allow.
      CheckContentSettings(ContentSettingsType::NOTIFICATIONS,
                           CONTENT_SETTING_ALLOW),
      // Go to block (not default!).
      TogglePermission(kFirstPermissionRow),
      // Verify the toggle is back off.
      CheckViewProperty(
          kFirstPermissionRow,
          &PermissionToggleRowView::GetToggleButtonStateForTesting, false),
      // Verify the content setting is Block.
      CheckContentSettings(ContentSettingsType::NOTIFICATIONS,
                           CONTENT_SETTING_BLOCK),
      // Verify that the subtitle is empty (for the block state).
      CheckViewProperty(kFirstPermissionRow,
                        &PermissionToggleRowView::GetRowSubTitleForTesting,
                        u""));
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewInteractiveUiTest,
                       BroadExceptionToggleTest_DefaultAllow) {
  ASSERT_EQ(host_content_settings_map()->GetDefaultContentSetting(
                ContentSettingsType::IMAGES),
            CONTENT_SETTING_ALLOW);
  // Set up a broad content setting exception for IMAGES.
  SetBroadException(ContentSettingsType::IMAGES, CONTENT_SETTING_BLOCK);

  RunTestSequenceInContext(
      context(), NavigateAndOpenPageInfo(),
      CheckViewProperty(PageInfoMainView::kMainLayoutElementId,
                        &PageInfoMainView::GetVisiblePermissionsCountForTesting,
                        1),
      // A view with permissions in PageInfo.
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      // Set id to the first children of `kPermissionsElementId` -
      // permissions view in PageInfo.
      NameChildView(PageInfoMainView::kPermissionsElementId,
                    kFirstPermissionRow, 0u),
      // Verify the row label is Images
      CheckViewProperty(
          kFirstPermissionRow, &PermissionToggleRowView::GetRowTitleForTesting,
          l10n_util::GetStringUTF16(IDS_SITE_SETTINGS_TYPE_IMAGES)),
      // Verify the toggle is off.
      CheckViewProperty(
          kFirstPermissionRow,
          &PermissionToggleRowView::GetToggleButtonStateForTesting, false),
      // Verify the content setting is Block.
      CheckContentSettings(ContentSettingsType::IMAGES, CONTENT_SETTING_BLOCK),
      // Toggle the button to allow.
      TogglePermission(kFirstPermissionRow),
      // Verify the toggle is on.
      CheckViewProperty(
          kFirstPermissionRow,
          &PermissionToggleRowView::GetToggleButtonStateForTesting, true),
      // Verify the content setting is Allow.
      CheckContentSettings(ContentSettingsType::IMAGES, CONTENT_SETTING_ALLOW),
      // Verify that the subtitle is empty (for "Allowed" state, as opposed to
      // "Allowed (default)").
      CheckViewProperty(kFirstPermissionRow,
                        &PermissionToggleRowView::GetRowSubTitleForTesting,
                        u""),
      // Return to block.
      TogglePermission(kFirstPermissionRow),
      // Verify the toggle is off.
      CheckViewProperty(
          kFirstPermissionRow,
          &PermissionToggleRowView::GetToggleButtonStateForTesting, false),
      // Verify the content setting is Block.
      CheckContentSettings(ContentSettingsType::IMAGES, CONTENT_SETTING_BLOCK));
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewInteractiveUiTest,
                       BroadExceptionToggleTest_DefaultBlock) {
  // Set Images permission to blocked by default.
  host_content_settings_map()->SetDefaultContentSetting(
      ContentSettingsType::IMAGES, CONTENT_SETTING_BLOCK);

  RunTestSequenceInContext(
      context(), NavigateAndOpenPageInfo(),
      CheckViewProperty(PageInfoMainView::kMainLayoutElementId,
                        &PageInfoMainView::GetVisiblePermissionsCountForTesting,
                        1),
      // A view with permissions in PageInfo.
      WaitForShow(PageInfoMainView::kPermissionsElementId),
      // Set id to the first children of `kPermissionsElementId` -
      // permissions view in PageInfo.
      NameChildView(PageInfoMainView::kPermissionsElementId,
                    kFirstPermissionRow, 0u),
      // Verify the row label is Images
      CheckViewProperty(
          kFirstPermissionRow, &PermissionToggleRowView::GetRowTitleForTesting,
          l10n_util::GetStringUTF16(IDS_SITE_SETTINGS_TYPE_IMAGES)),
      // Verify the toggle is off.
      CheckViewProperty(
          kFirstPermissionRow,
          &PermissionToggleRowView::GetToggleButtonStateForTesting, false),
      // Verify the content setting is Block.
      CheckContentSettings(ContentSettingsType::IMAGES, CONTENT_SETTING_BLOCK),
      // Verify that the subtitle for the block (default) state.
      CheckViewProperty(kFirstPermissionRow,
                        &PermissionToggleRowView::GetRowSubTitleForTesting,
                        u"Not allowed (default)"),
      // Toggle the button to allow.
      TogglePermission(kFirstPermissionRow),
      // Verify the toggle is on.
      CheckViewProperty(
          kFirstPermissionRow,
          &PermissionToggleRowView::GetToggleButtonStateForTesting, true),
      // Verify the content setting is Allow.
      CheckContentSettings(ContentSettingsType::IMAGES, CONTENT_SETTING_ALLOW),
      // Go to block (not default!).
      TogglePermission(kFirstPermissionRow),
      // Verify the toggle is off.
      CheckViewProperty(
          kFirstPermissionRow,
          &PermissionToggleRowView::GetToggleButtonStateForTesting, false),
      // Verify the content setting is Block.
      CheckContentSettings(ContentSettingsType::IMAGES, CONTENT_SETTING_BLOCK),
      // Verify that the subtitle is empty (for the block state).
      CheckViewProperty(kFirstPermissionRow,
                        &PermissionToggleRowView::GetRowSubTitleForTesting,
                        u""));
}
