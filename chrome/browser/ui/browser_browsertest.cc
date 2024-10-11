// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/browser.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "ash/constants/web_app_id_constants.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_ui_prefs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/startup/launch_mode_recorder.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "chrome/browser/ui/startup/web_app_startup_utils.h"
#include "chrome/browser/ui/tabs/pinned_tab_codec.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/embedder_support/switches.h"
#include "components/javascript_dialogs/app_modal_dialog_controller.h"
#include "components/javascript_dialogs/app_modal_dialog_queue.h"
#include "components/javascript_dialogs/app_modal_dialog_view.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/sessions/core/command_storage_manager_test_helper.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/slow_http_response.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/ui_base_features.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/scoped_nsautorelease_pool.h"
#include "chrome/browser/ui/cocoa/test/run_loop_testing.h"
#include "ui/accelerated_widget_mac/ca_transaction_observer.h"
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/i18n/rtl.h"
#include "base/test/file_path_reparse_point_win.h"
#endif

using base::ASCIIToUTF16;
using content::HostZoomMap;
using content::NavigationController;
using content::NavigationEntry;
using content::OpenURLParams;
using content::Referrer;
using content::WebContents;
using content::WebContentsObserver;
using extensions::Extension;
using javascript_dialogs::AppModalDialogController;
using javascript_dialogs::AppModalDialogQueue;

namespace {

const char* kBeforeUnloadHTML =
    "<html><head><title>beforeunload</title></head><body>"
    "<script>window.onbeforeunload=function(e){return 'foo'}</script>"
    "</body></html>";

const char16_t* kOpenNewBeforeUnloadPage =
    u"w=window.open(); w.onbeforeunload=function(e){return 'foo'};";

const base::FilePath::CharType* kTitle1File = FILE_PATH_LITERAL("title1.html");
const base::FilePath::CharType* kTitle2File = FILE_PATH_LITERAL("title2.html");

// Given a page title, returns the expected window caption string.
std::u16string WindowCaptionFromPageTitle(const std::u16string& page_title) {
#if BUILDFLAG(IS_MAC)
  // On Mac, we don't want to suffix the page title with the application name.
  if (page_title.empty())
    return l10n_util::GetStringUTF16(IDS_BROWSER_WINDOW_MAC_TAB_UNTITLED);
  return page_title;
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // On Lacros, we don't want to suffix the page title with the application
  // name. Note that the default title for empty title is different from Mac.
  if (page_title.empty()) {
    return l10n_util::GetStringUTF16(IDS_DEFAULT_TAB_TITLE);
  }
  return page_title;
#else
  if (page_title.empty())
    return l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);

  return l10n_util::GetStringFUTF16(IDS_BROWSER_WINDOW_TITLE_FORMAT,
                                    page_title);
#endif
}

// Returns the number of active RenderProcessHosts.
int CountRenderProcessHosts() {
  int result = 0;
  for (content::RenderProcessHost::iterator i(
           content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance())
    ++result;
  return result;
}

class TabClosingObserver : public TabStripModelObserver {
 public:
  TabClosingObserver() = default;
  TabClosingObserver(const TabClosingObserver&) = delete;
  TabClosingObserver& operator=(const TabClosingObserver&) = delete;

  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() != TabStripModelChange::kRemoved)
      return;

    auto* remove = change.GetRemove();
    for (const auto& contents : remove->contents) {
      if (contents.remove_reason == TabStripModelChange::RemoveReason::kDeleted)
        closing_count_ += 1;
    }
  }

  int closing_count() const { return closing_count_; }

 private:
  int closing_count_ = 0;
};

// Used by CloseWithAppMenuOpen. Posts a CloseWindowCallback and shows the app
// menu.
void RunCloseWithAppMenuCallback(Browser* browser) {
  // ShowAppMenu is modal under views. Schedule a task that closes the window.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&chrome::CloseWindow, browser));
  chrome::ShowAppMenu(browser);
}

class RenderViewSizeObserver : public content::WebContentsObserver {
 public:
  RenderViewSizeObserver(content::WebContents* web_contents,
                         BrowserWindow* browser_window)
      : WebContentsObserver(web_contents), browser_window_(browser_window) {}
  RenderViewSizeObserver(const RenderViewSizeObserver&) = delete;
  RenderViewSizeObserver& operator=(const RenderViewSizeObserver&) = delete;

  void GetSizeForRenderViewHost(content::RenderViewHost* render_view_host,
                                gfx::Size* rwhv_create_size,
                                gfx::Size* rwhv_commit_size,
                                gfx::Size* wcv_commit_size) {
    RenderViewSizes::const_iterator result = render_view_sizes_.end();
    result = render_view_sizes_.find(render_view_host);
    if (result != render_view_sizes_.end()) {
      *rwhv_create_size = result->second.rwhv_create_size;
      *rwhv_commit_size = result->second.rwhv_commit_size;
      *wcv_commit_size = result->second.wcv_commit_size;
    }
  }

  void set_wcv_resize_insets(const gfx::Size& wcv_resize_insets) {
    wcv_resize_insets_ = wcv_resize_insets;
  }

  // Cache the size when RenderViewHost's main frame is first created.
  void RenderFrameCreated(
      content::RenderFrameHost* render_frame_host) override {
    if (!render_frame_host->GetParent()) {
      content::RenderViewHost* render_view_host =
          render_frame_host->GetRenderViewHost();
      render_view_sizes_[render_view_host].rwhv_create_size =
          render_view_host->GetWidget()->GetView()->GetViewBounds().size();
    }
  }

  // Enlarge WebContentsView by |wcv_resize_insets_| while the navigation entry
  // is pending. Since the new render process may not be created when the
  // navigation starts if the feature DeferSpeculativeRFHCreation is enabled,
  // resize the window when the navigation is ready to commit. Otherwise we will
  // change the size of the original window.
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override {
    Resize();
  }

  void Resize() {
    if (wcv_resize_insets_.IsEmpty())
      return;
    // Resizing the main browser window by |wcv_resize_insets_| will
    // automatically resize the WebContentsView by the same amount.
    // Just resizing WebContentsView directly doesn't work on Linux, because the
    // next automatic layout of the browser window will resize WebContentsView
    // back to the previous size.  To make it consistent, resize main browser
    // window on all platforms.
    gfx::Rect bounds(browser_window_->GetBounds());
    gfx::Size size(bounds.size());
    size.Enlarge(wcv_resize_insets_.width(), wcv_resize_insets_.height());
    bounds.set_size(size);
    browser_window_->SetBounds(bounds);
    // Let the message loop run so that resize actually takes effect.
    content::RunAllPendingInMessageLoop();
  }

  // Cache the sizes of RenderWidgetHostView and WebContentsView when the
  // navigation entry is committed, which is before
  // WebContentsObserver::DidFinishNavigation is called.
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& details) override {
    content::RenderViewHost* rvh =
        web_contents()->GetPrimaryMainFrame()->GetRenderViewHost();
    render_view_sizes_[rvh].rwhv_commit_size =
        web_contents()->GetRenderWidgetHostView()->GetViewBounds().size();
    render_view_sizes_[rvh].wcv_commit_size =
        web_contents()->GetContainerBounds().size();
  }

 private:
  struct Sizes {
    gfx::Size rwhv_create_size;  // Size of RenderWidgetHostView when created.
    gfx::Size rwhv_commit_size;  // Size of RenderWidgetHostView when committed.
    gfx::Size wcv_commit_size;   // Size of WebContentsView when committed.
  };

  typedef std::map<content::RenderViewHost*, Sizes> RenderViewSizes;
  RenderViewSizes render_view_sizes_;
  // Enlarge WebContentsView by this size insets in
  // DidStartNavigation.
  gfx::Size wcv_resize_insets_;
  raw_ptr<BrowserWindow> browser_window_;  // Weak ptr.
};

}  // namespace

class BrowserTest : public extensions::ExtensionBrowserTest,
                    public BrowserListObserver {
 protected:
  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  // In RTL locales wrap the page title with RTL embedding characters so that it
  // matches the value returned by GetWindowTitle().
  std::u16string LocaleWindowCaptionFromPageTitle(
      const std::u16string& expected_title) {
    std::u16string page_title = WindowCaptionFromPageTitle(expected_title);
#if BUILDFLAG(IS_WIN)
    std::string locale = g_browser_process->GetApplicationLocale();
    if (base::i18n::GetTextDirectionForLocale(locale.c_str()) ==
        base::i18n::RIGHT_TO_LEFT) {
      base::i18n::WrapStringWithLTRFormatting(&page_title);
    }

    return page_title;
#else
    // Do we need to use the above code on POSIX as well?
    return page_title;
#endif
  }

  void OpenURLFromTab(WebContents* source, OpenURLParams params) {
    browser()->OpenURLFromTab(source, params,
                              /*navigation_handle_callback=*/{});
  }

  // Returns the app extension aptly named "App Test".
  const Extension* GetExtension() {
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(browser()->profile());
    for (const scoped_refptr<const extensions::Extension>& extension :
         registry->enabled_extensions()) {
      if (extension->name() == "App Test")
        return extension.get();
    }
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  // BrowserListObserver:
  MOCK_METHOD(void,
              OnBrowserCloseCancelled,
              (Browser * browser, BrowserClosingStatus reason),
              (override));

 private:
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
};

// Launch the app on a page with no title, check that the app title was set
// correctly.
IN_PROC_BROWSER_TEST_F(BrowserTest, NoTitle) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), ui_test_utils::GetTestUrl(
                     base::FilePath(base::FilePath::kCurrentDirectory),
                     base::FilePath(kTitle1File))));
  EXPECT_EQ(
      LocaleWindowCaptionFromPageTitle(u"title1.html"),
      browser()->GetWindowTitleForCurrentTab(true /* include_app_name */));
  std::u16string tab_title;
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &tab_title));
  EXPECT_EQ(u"title1.html", tab_title);
}

// Check that a file:// URL displays the filename, but no path, with any ref or
// query parameters following it if the content does not have a <title> tag.
// Specifically verify the cases where the ref or query parameters have a '/'
// character in them. This is a regression test for
// https://crbug.com/503003.
IN_PROC_BROWSER_TEST_F(BrowserTest, NoTitleFileUrl) {
  // Note that the host names used and the order of these cases are by design.
  // There must be unique query parameters and references per case (i.e. the
  // indexed foo*.com hosts) because if the same query parameter is repeated in
  // a row, then the navigation may not actually happen, as it will only appear
  // as a reference change.  Additionally, cases with references first must
  // appear after a query parameter case since otherwise it will not be a
  // navigation.
  struct {
    std::string suffix;
    std::string message;
  } cases[]{
      {"#https://foo1.com", "file:/// URL with slash in ref"},
      {"?x=https://foo2.com", "file:/// URL with slash in query parameter"},
      {"?x=https://foo3.com#https://foo3.com",
       "file:/// URL with slashes in query parameter and ref"},
      {"#https://foo4.com?x=https://foo4.com",
       "file:/// URL with slashes in ref and query parameter"},
      {"?x=https://foo6.com?x=https://foo6.com",
       "file:/// URL with slashes in multiple query parameter"},
      {"#https://foo5.com#https://foo5.com",
       "file:/// URL with slashes in multiple refs"}};

  GURL prefix_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kTitle1File));
  std::u16string tab_title;
  std::u16string test_title;
  for (const auto& c : cases) {
    SCOPED_TRACE(c.message);
    GURL url(prefix_url.spec() + c.suffix);
    test_title = u"title1.html" + ASCIIToUTF16(c.suffix);
    content::TitleWatcher title_watcher(
        browser()->tab_strip_model()->GetActiveWebContents(), test_title);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_EQ(test_title, title_watcher.WaitAndGetTitle());
  }
}

// Launch the app, navigate to a page with a title, check that the app title
// was set correctly.
IN_PROC_BROWSER_TEST_F(BrowserTest, Title) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), ui_test_utils::GetTestUrl(
                     base::FilePath(base::FilePath::kCurrentDirectory),
                     base::FilePath(kTitle2File))));
  const std::u16string test_title(u"Title Of Awesomeness");
  EXPECT_EQ(
      LocaleWindowCaptionFromPageTitle(test_title),
      browser()->GetWindowTitleForCurrentTab(true /* include_app_name */));
  std::u16string tab_title;
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &tab_title));
  EXPECT_EQ(test_title, tab_title);
}

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
// Check that the title is different when a page is opened in a captive portal
// window.
IN_PROC_BROWSER_TEST_F(BrowserTest, CaptivePortalWindowTitle) {
  const GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kTitle2File));
  NavigateParams captive_portal_params(browser(), url,
                                       ui::PAGE_TRANSITION_TYPED);
  captive_portal_params.disposition = WindowOpenDisposition::NEW_POPUP;
  captive_portal_params.captive_portal_window_type =
      captive_portal::CaptivePortalWindowType::kPopup;
  ui_test_utils::NavigateToURL(&captive_portal_params);
  std::u16string captive_portal_window_title =
      chrome::FindBrowserWithTab(
          captive_portal_params.navigated_or_inserted_contents)
          ->GetWindowTitleForCurrentTab(true /* include_app_name */);

  NavigateParams normal_params(browser(), url, ui::PAGE_TRANSITION_TYPED);
  normal_params.disposition = WindowOpenDisposition::NEW_POPUP;
  ui_test_utils::NavigateToURL(&normal_params);
  std::u16string normal_window_title =
      chrome::FindBrowserWithTab(normal_params.navigated_or_inserted_contents)
          ->GetWindowTitleForCurrentTab(true /* include_app_name */);

  ASSERT_NE(captive_portal_window_title, normal_window_title);
}
#endif

IN_PROC_BROWSER_TEST_F(BrowserTest, NoJavaScriptDialogsActivateTab) {
  // Set up two tabs, with the tab at index 0 active.
  GURL url(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kTitle1File)));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  WebContents* second_tab = browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_TRUE(second_tab);

  // Show a confirm() dialog from the tab at index 1. The active index shouldn't
  // budge.
  {
    content::WebContentsConsoleObserver confirm_observer(second_tab);
    confirm_observer.SetPattern("*confirm*suppressed*");
    second_tab->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        u"confirm('Activate!');", base::NullCallback(),
        content::ISOLATED_WORLD_ID_GLOBAL);
    ASSERT_TRUE(confirm_observer.Wait());
  }
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  // Show a prompt() dialog from the tab at index 1. The active index shouldn't
  // budge.
  {
    content::WebContentsConsoleObserver prompt_observer(second_tab);
    prompt_observer.SetPattern("*prompt*suppressed*");
    second_tab->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        u"prompt('Activate!');", base::NullCallback(),
        content::ISOLATED_WORLD_ID_GLOBAL);
    ASSERT_TRUE(prompt_observer.Wait());
  }
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  // Show an alert() dialog from the tab at index 1. The active index shouldn't
  // budge.
  auto* js_dialog_manager =
      javascript_dialogs::TabModalDialogManager::FromWebContents(second_tab);
  base::RunLoop alert_wait;
  js_dialog_manager->SetDialogShownCallbackForTesting(alert_wait.QuitClosure());
  second_tab->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"alert('Activate!');", base::NullCallback(),
      content::ISOLATED_WORLD_ID_GLOBAL);
  alert_wait.Run();
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
}

// Create 34 tabs and verify that a lot of processes have been created. The
// exact number of processes depends on the amount of memory. Previously we
// had a hard limit of 31 processes and this test is mainly directed at
// verifying that we don't crash when we pass this limit.
// Warning: this test can take >30 seconds when running on a slow (low
// memory?) Mac builder.
// Test is flaky on Win, Linux, Mac: https://crbug.com/1099186.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
#define MAYBE_ThirtyFourTabs DISABLED_ThirtyFourTabs
#else
#define MAYBE_ThirtyFourTabs ThirtyFourTabs
#endif
IN_PROC_BROWSER_TEST_F(BrowserTest, MAYBE_ThirtyFourTabs) {
  GURL url(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kTitle2File)));

  // There is one initial tab.
  const int kTabCount = 34;
  for (int ix = 0; ix != (kTabCount - 1); ++ix) {
    chrome::AddSelectedTabWithURL(browser(), url, ui::PAGE_TRANSITION_TYPED);
  }
  EXPECT_EQ(kTabCount, browser()->tab_strip_model()->count());

  // See GetMaxRendererProcessCount() in
  // content/browser/renderer_host/render_process_host_impl.cc
  // for the algorithm to decide how many processes to create.
  const int kExpectedProcessCount =
#if defined(ARCH_CPU_64_BITS)
      12;
#else
      17;
#endif
  if (base::SysInfo::AmountOfPhysicalMemoryMB() >= 2048) {
    EXPECT_GE(CountRenderProcessHosts(), kExpectedProcessCount);
  } else {
    EXPECT_LT(CountRenderProcessHosts(), kExpectedProcessCount);
  }
}

// Test that a browser-initiated navigation to an aborted URL load leaves around
// a pending entry if we start from the NTP but not from a normal page.
// See http://crbug.com/355537.
IN_PROC_BROWSER_TEST_F(BrowserTest, ClearPendingOnFailUnlessNTP) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));

  // Navigate to a 204 URL (aborts with no content) on the NTP and make sure it
  // sticks around so that the user can edit it.
  GURL abort_url(embedded_test_server()->GetURL("/nocontent"));
  {
    content::LoadStopObserver stop_observer(web_contents);
    browser()->OpenURL(
        OpenURLParams(abort_url, Referrer(), WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false),
        /*navigation_handle_callback=*/{});
    stop_observer.Wait();
    EXPECT_TRUE(web_contents->GetController().GetPendingEntry());
    EXPECT_EQ(abort_url, web_contents->GetVisibleURL());
  }

  // Navigate to a real URL.
  GURL real_url(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), real_url));
  EXPECT_EQ(real_url, web_contents->GetVisibleURL());

  // Now navigating to a 204 URL should clear the pending entry.
  {
    content::LoadStopObserver stop_observer(web_contents);
    browser()->OpenURL(
        OpenURLParams(abort_url, Referrer(), WindowOpenDisposition::CURRENT_TAB,
                      ui::PAGE_TRANSITION_TYPED, false),
        /*navigation_handle_callback=*/{});
    stop_observer.Wait();
    EXPECT_FALSE(web_contents->GetController().GetPendingEntry());
    EXPECT_EQ(real_url, web_contents->GetVisibleURL());
  }
}

// Test for crbug.com/1232447. Ensure that a non-user-initiated navigation
// doesn't commit while a JS dialog is showing.
IN_PROC_BROWSER_TEST_F(BrowserTest, DialogDefersNavigationCommit) {
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL kEmptyUrl(embedded_test_server()->GetURL("/empty.html"));
  const GURL kSecondUrl(embedded_test_server()->GetURL("/title1.html"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kEmptyUrl));

  content::TestNavigationManager manager(contents, kSecondUrl);
  auto* js_dialog_manager =
      javascript_dialogs::TabModalDialogManager::FromWebContents(contents);

  // Start a non-user-gesture navigation to the second page but block after the
  // request is started.
  {
    auto script = content::JsReplace("window.location = $1;", kSecondUrl);
    ASSERT_TRUE(content::ExecJs(contents->GetPrimaryMainFrame(), script,
                                content::EXECUTE_SCRIPT_NO_USER_GESTURE));
    ASSERT_TRUE(manager.WaitForRequestStart());
  }

  // Show a modal JavaScript dialog.
  {
    base::RunLoop run_loop;

    js_dialog_manager->SetDialogShownCallbackForTesting(run_loop.QuitClosure());
    contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        u"alert('one'); ", base::NullCallback(),
        content::ISOLATED_WORLD_ID_GLOBAL);
    run_loop.Run();

    ASSERT_TRUE(js_dialog_manager->IsShowingDialogForTesting());
  }

  // Continue the navigation through the response and on to commit. Since a
  // dialog is showing, this should cause the navigation to be deferred before
  // commit and the dialog should remain showing.
  {
    ASSERT_TRUE(manager.WaitForResponse());
    manager.ResumeNavigation();

    content::NavigationHandle* handle = manager.GetNavigationHandle();
    EXPECT_FALSE(handle->IsWaitingToCommit());
    EXPECT_TRUE(handle->IsCommitDeferringConditionDeferredForTesting());
    EXPECT_TRUE(js_dialog_manager->IsShowingDialogForTesting());
  }

  // Dismiss the dialog. This should resume the navigation.
  {
    js_dialog_manager->ClickDialogButtonForTesting(true, std::u16string());
    ASSERT_FALSE(js_dialog_manager->IsShowingDialogForTesting());

    content::NavigationHandle* handle = manager.GetNavigationHandle();
    EXPECT_FALSE(handle->IsCommitDeferringConditionDeferredForTesting());
    EXPECT_TRUE(handle->IsWaitingToCommit());
  }

  ASSERT_TRUE(manager.WaitForNavigationFinished());
}

// Test for crbug.com/297289.  Ensure that modal dialogs are closed when a
// cross-process navigation is ready to commit.
IN_PROC_BROWSER_TEST_F(BrowserTest, CrossProcessNavCancelsDialogs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Test this with multiple alert dialogs to ensure that we can navigate away
  // even if the renderer tries to synchronously create more.
  // See http://crbug.com/312490.
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  auto* js_dialog_manager =
      javascript_dialogs::TabModalDialogManager::FromWebContents(contents);
  base::RunLoop dialog_wait;
  js_dialog_manager->SetDialogShownCallbackForTesting(
      dialog_wait.QuitClosure());
  contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"alert('one'); alert('two');", base::NullCallback(),
      content::ISOLATED_WORLD_ID_GLOBAL);
  dialog_wait.Run();
  EXPECT_TRUE(js_dialog_manager->IsShowingDialogForTesting());

  // A cross-site navigation should force the dialog to close.
  GURL url2("http://www.example.com/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));
  EXPECT_FALSE(js_dialog_manager->IsShowingDialogForTesting());

  // Make sure input events still work in the renderer process.
  EXPECT_FALSE(contents->GetPrimaryMainFrame()->GetProcess()->IsBlocked());
}

// Similar to CrossProcessNavCancelsDialogs, with a renderer-initiated main
// frame navigation with user gesture.
IN_PROC_BROWSER_TEST_F(BrowserTest, RendererCrossProcessNavCancelsDialogs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();

  // A cross-site renderer-initiated navigation with user gesture (started
  // before the dialog is shown) should force the dialog to close. (ExecJS sends
  // a user gesture by default.)
  GURL url2("http://www.example.com/empty.html");
  content::TestNavigationManager manager(contents, url2);
  EXPECT_TRUE(content::ExecJs(contents, "location = '" + url2.spec() + "';"));
  EXPECT_TRUE(manager.WaitForRequestStart());

  auto* js_dialog_manager =
      javascript_dialogs::TabModalDialogManager::FromWebContents(contents);
  base::RunLoop dialog_wait;
  js_dialog_manager->SetDialogShownCallbackForTesting(
      dialog_wait.QuitClosure());
  content::ExecuteScriptAsync(contents, "alert('dialog')");
  dialog_wait.Run();
  EXPECT_TRUE(js_dialog_manager->IsShowingDialogForTesting());

  // Let the navigation to url2 finish and dismiss the dialog.
  ASSERT_TRUE(manager.WaitForNavigationFinished());
  EXPECT_FALSE(js_dialog_manager->IsShowingDialogForTesting());

  // Make sure input events still work in the renderer process.
  EXPECT_FALSE(contents->GetPrimaryMainFrame()->GetProcess()->IsBlocked());
}

// Ensures that a download can complete while a dialog is showing, because it
// poses no risk of dismissing the dialog.
IN_PROC_BROWSER_TEST_F(BrowserTest, DownloadDoesntDismissDialog) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();

  // A renderer-initiated navigation without a user gesture would normally be
  // deferred until the dialog is dismissed.  If the navigation turns out to be
  // a download at response time (e.g., because download-test3.gif has a
  // Content-Disposition: attachment response header), then the download should
  // not be deferred or dismiss the dialog.
  std::unique_ptr<content::DownloadTestObserver> download_waiter(
      new content::DownloadTestObserverTerminal(
          browser()->profile()->GetDownloadManager(), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));
  GURL url2(embedded_test_server()->GetURL("/download-test3.gif"));
  content::TestNavigationManager manager(contents, url2);
  EXPECT_TRUE(content::ExecJs(contents, "location = '" + url2.spec() + "';",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_TRUE(manager.WaitForRequestStart());

  // Show a dialog while we're waiting for the url2 response.
  auto* js_dialog_manager =
      javascript_dialogs::TabModalDialogManager::FromWebContents(contents);
  base::RunLoop dialog_wait;
  js_dialog_manager->SetDialogShownCallbackForTesting(
      dialog_wait.QuitClosure());
  content::ExecuteScriptAsync(contents, "alert('dialog')");
  dialog_wait.Run();
  EXPECT_TRUE(js_dialog_manager->IsShowingDialogForTesting());

  // Let the url2 response finish and become a download, without dismissing the
  // dialog.
  ASSERT_TRUE(manager.WaitForNavigationFinished());
  EXPECT_TRUE(js_dialog_manager->IsShowingDialogForTesting());
  download_waiter->WaitForFinished();

  // Close the dialog after the download finishes, to clean up.
  js_dialog_manager->ClickDialogButtonForTesting(true, std::u16string());
  EXPECT_FALSE(js_dialog_manager->IsShowingDialogForTesting());

  // Make sure input events still work in the renderer process.
  EXPECT_FALSE(contents->GetPrimaryMainFrame()->GetProcess()->IsBlocked());
}

#if BUILDFLAG(IS_MAC)
// Flaky on Mac 10.11 CI builder. See https://crbug.com/1251684.
#define MAYBE_SadTabCancelsDialogs DISABLED_SadTabCancelsDialogs
#else
#define MAYBE_SadTabCancelsDialogs SadTabCancelsDialogs
#endif

// Make sure that dialogs are closed after a renderer process dies, and that
// subsequent navigations work.  See http://crbug/com/343265.
IN_PROC_BROWSER_TEST_F(BrowserTest, MAYBE_SadTabCancelsDialogs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL beforeunload_url(embedded_test_server()->GetURL("/beforeunload.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), beforeunload_url));
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::PrepContentsForBeforeUnloadTest(contents);

  // Start a navigation to trigger the beforeunload dialog.
  contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"window.location.href = 'about:blank'", base::NullCallback(),
      content::ISOLATED_WORLD_ID_GLOBAL);
  AppModalDialogController* alert = ui_test_utils::WaitForAppModalDialog();
  EXPECT_TRUE(alert->IsValid());
  AppModalDialogQueue* dialog_queue = AppModalDialogQueue::GetInstance();
  EXPECT_TRUE(dialog_queue->HasActiveDialog());

  // Crash the renderer process and ensure the dialog is gone.
  content::RenderProcessHost* child_process =
      contents->GetPrimaryMainFrame()->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      child_process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0);
  crash_observer.Wait();
  EXPECT_FALSE(dialog_queue->HasActiveDialog());

  // Make sure subsequent navigations work.
  GURL url2("http://www.example.com/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));
}

// Make sure that dialogs opened by subframes are closed when the process dies.
// See http://crbug.com/366510.
IN_PROC_BROWSER_TEST_F(BrowserTest, SadTabCancelsSubframeDialogs) {
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html, <html><body></body></html>")));

  // Create an iframe that opens an alert dialog.
  auto* js_dialog_manager =
      javascript_dialogs::TabModalDialogManager::FromWebContents(contents);
  base::RunLoop dialog_wait;
  js_dialog_manager->SetDialogShownCallbackForTesting(
      dialog_wait.QuitClosure());
  contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"f = document.createElement('iframe');"
      u"f.srcdoc = '<script>alert(1)</script>';"
      u"document.body.appendChild(f);",
      base::NullCallback(), content::ISOLATED_WORLD_ID_GLOBAL);
  dialog_wait.Run();
  EXPECT_TRUE(js_dialog_manager->IsShowingDialogForTesting());

  // Crash the renderer process and ensure the dialog is gone.
  content::RenderProcessHost* child_process =
      contents->GetPrimaryMainFrame()->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      child_process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0);
  crash_observer.Wait();
  EXPECT_FALSE(js_dialog_manager->IsShowingDialogForTesting());

  // Make sure subsequent navigations work.
  GURL url2("data:text/html,foo");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));
}

// Test for crbug.com/22004.  Reloading a page with a before unload handler and
// then canceling the dialog should not leave the throbber spinning.
// https://crbug.com/898370: Test is flakily timing out
IN_PROC_BROWSER_TEST_F(BrowserTest, DISABLED_ReloadThenCancelBeforeUnload) {
  GURL url(std::string("data:text/html,") + kBeforeUnloadHTML);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::PrepContentsForBeforeUnloadTest(contents);

  // Navigate to another page, but click cancel in the dialog.  Make sure that
  // the throbber stops spinning.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  AppModalDialogController* alert = ui_test_utils::WaitForAppModalDialog();

  alert->CloseModalDialog();
  EXPECT_FALSE(contents->IsLoading());

  // Clear the beforeunload handler so the test can easily exit.
  contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"onbeforeunload=null;", base::NullCallback(),
      content::ISOLATED_WORLD_ID_GLOBAL);
}

// Test for crbug.com/11647.  A page closed with window.close() should not have
// two beforeunload dialogs shown.
// http://crbug.com/410891
IN_PROC_BROWSER_TEST_F(BrowserTest,
                       DISABLED_SingleBeforeUnloadAfterWindowClose) {
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->GetPrimaryMainFrame()
      ->ExecuteJavaScriptWithUserGestureForTests(
          kOpenNewBeforeUnloadPage, base::NullCallback(),
          content::ISOLATED_WORLD_ID_GLOBAL);

  // Close the new window with JavaScript, which should show a single
  // beforeunload dialog.  Then show another alert, to make it easy to verify
  // that a second beforeunload dialog isn't shown.
  browser()
      ->tab_strip_model()
      ->GetWebContentsAt(0)
      ->GetPrimaryMainFrame()
      ->ExecuteJavaScriptWithUserGestureForTests(
          u"w.close(); alert('bar');", base::NullCallback(),
          content::ISOLATED_WORLD_ID_GLOBAL);
  AppModalDialogController* alert = ui_test_utils::WaitForAppModalDialog();
  alert->view()->AcceptAppModalDialog();

  alert = ui_test_utils::WaitForAppModalDialog();
  EXPECT_FALSE(alert->is_before_unload_dialog());
  alert->view()->AcceptAppModalDialog();
}

// Test that when a page has an onbeforeunload handler, reloading a page shows a
// different dialog than navigating to a different page.
IN_PROC_BROWSER_TEST_F(BrowserTest, BeforeUnloadVsBeforeReload) {
  GURL url(std::string("data:text/html,") + kBeforeUnloadHTML);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::PrepContentsForBeforeUnloadTest(contents);

  // Reload the page, and check that we get a "before reload" dialog.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  AppModalDialogController* alert = ui_test_utils::WaitForAppModalDialog();
  EXPECT_TRUE(alert->is_reload());

  // Proceed with the reload.
  alert->view()->AcceptAppModalDialog();
  EXPECT_TRUE(content::WaitForLoadStop(contents));

  content::PrepContentsForBeforeUnloadTest(contents);

  // Navigate to another url, and check that we get a "before unload" dialog.
  GURL url2(url::kAboutBlankURL);
  browser()->OpenURL(
      OpenURLParams(url2, Referrer(), WindowOpenDisposition::CURRENT_TAB,
                    ui::PAGE_TRANSITION_TYPED, false),
      /*navigation_handle_callback=*/{});

  alert = ui_test_utils::WaitForAppModalDialog();
  EXPECT_FALSE(alert->is_reload());

  // Accept the navigation so we end up on a page without a beforeunload hook.
  alert->view()->AcceptAppModalDialog();
}

// TODO(crbug.com/40641945): Test this with implicitly-created links.
IN_PROC_BROWSER_TEST_F(BrowserTest, TargetBlankLinkOpensInGroup) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Add a grouped tab.
  TabStripModel* const model = browser()->tab_strip_model();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/frame_tree/anchor_to_same_site_location.html")));
  const tab_groups::TabGroupId group_id = model->AddToNewGroup({0});

  // Click a target=_blank link.
  WebContents* const contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      ExecJs(contents, "simulateClick(\"test-anchor-with-blank-target\", {})"));

  // The new tab should have inherited the tab group from the first tab.
  EXPECT_EQ(group_id, browser()->tab_strip_model()->GetTabGroupForTab(1));
}

IN_PROC_BROWSER_TEST_F(BrowserTest, NewTabFromLinkInGroupedTabOpensInGroup) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Add a grouped tab.
  TabStripModel* const model = browser()->tab_strip_model();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));
  const tab_groups::TabGroupId group_id = model->AddToNewGroup({0});

  // Open a new background tab.
  WebContents* const contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  OpenURLFromTab(
      contents,
      OpenURLParams(embedded_test_server()->GetURL("/empty.html"), Referrer(),
                    WindowOpenDisposition::NEW_BACKGROUND_TAB,
                    ui::PAGE_TRANSITION_TYPED, false));

  // It should have inherited the tab group from the first tab.
  EXPECT_EQ(group_id, model->GetTabGroupForTab(1));
}

// Tests that other popup navigations that do not follow the steps at
// http://www.google.com/chrome/intl/en/webmasters-faq.html#newtab will not
// fork a new renderer process.
IN_PROC_BROWSER_TEST_F(BrowserTest, OtherRedirectsDontForkProcess) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      embedder_support::kDisablePopupBlocking);

  // Create http and https servers for a cross-site transition.
  ASSERT_TRUE(embedded_test_server()->Start());
  net::EmbeddedTestServer https_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(https_test_server.Start());
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  GURL https_url(https_test_server.GetURL("/title2.html"));

  // Start with an http URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), http_url));
  WebContents* oldtab = browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderProcessHost* process =
      oldtab->GetPrimaryMainFrame()->GetProcess();

  // Now open a tab to a blank page and redirect it cross-site.
  std::string dont_fork_popup = "w=window.open();";
  dont_fork_popup += "w.document.location=\"";
  dont_fork_popup += https_url.spec();
  dont_fork_popup += "\";";

  ui_test_utils::TabAddedWaiter tab_add(browser());
  EXPECT_TRUE(content::ExecJs(oldtab->GetPrimaryMainFrame(), dont_fork_popup));

  // The tab should be created by the time the script finished running.
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  WebContents* newtab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(newtab);
  EXPECT_NE(oldtab, newtab);

  // New tab should be in the middle of document.location navigation.
  EXPECT_TRUE(newtab->IsLoading());
  content::WaitForLoadStop(newtab);

  ASSERT_TRUE(newtab->GetController().GetLastCommittedEntry());
  EXPECT_EQ(https_url.spec(),
            newtab->GetController().GetLastCommittedEntry()->GetURL().spec());

  // Process of the (cross-site) popup window depends on whether
  // site-per-process mode is enabled or not.
  content::RenderProcessHost* popup_process =
      newtab->GetPrimaryMainFrame()->GetProcess();
  if (content::AreAllSitesIsolatedForTesting())
    EXPECT_NE(process, popup_process);
  else
    EXPECT_EQ(process, popup_process);

  // Same thing if the current tab tries to navigate itself.
  std::string navigate_str = "document.location=\"";
  navigate_str += https_url.spec();
  navigate_str += "\";";
  EXPECT_TRUE(content::ExecJs(oldtab->GetPrimaryMainFrame(), navigate_str));

  // The old tab should be in the middle of document.location navigation.
  EXPECT_TRUE(oldtab->IsLoading());
  content::WaitForLoadStop(oldtab);

  ASSERT_TRUE(oldtab->GetController().GetLastCommittedEntry());
  EXPECT_EQ(https_url.spec(),
            oldtab->GetController().GetLastCommittedEntry()->GetURL().spec());

  // Whether original stays in the original process (when navigating to a
  // cross-site url) depends on whether site-per-process mode is enabled or not.
  content::RenderProcessHost* new_process =
      newtab->GetPrimaryMainFrame()->GetProcess();
  if (content::AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(process, new_process);

    // site-per-process should reuse the process for the https site.
    EXPECT_EQ(popup_process, new_process);
  } else {
    EXPECT_EQ(process, new_process);
  }
}

// Test RenderView correctly send back favicon url for web page that redirects
// to an anchor in javascript body.onload handler.
IN_PROC_BROWSER_TEST_F(BrowserTest,
                       DISABLED_FaviconOfOnloadRedirectToAnchorPage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/onload_redirect_to_anchor.html"));
  GURL expected_favicon_url(embedded_test_server()->GetURL("/test.png"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  NavigationEntry* entry = browser()
                               ->tab_strip_model()
                               ->GetActiveWebContents()
                               ->GetController()
                               .GetLastCommittedEntry();
  EXPECT_EQ(expected_favicon_url.spec(), entry->GetFavicon().url.spec());
}

// Makes sure TabClosing is sent when uninstalling an extension that is an app
// tab.
IN_PROC_BROWSER_TEST_F(BrowserTest, TabClosingWhenRemovingExtension) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/empty.html"));
  TabStripModel* model = browser()->tab_strip_model();

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app/")));

  const Extension* extension_app = GetExtension();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  std::unique_ptr<WebContents> app_contents =
      WebContents::Create(WebContents::CreateParams(browser()->profile()));
  extensions::TabHelper::CreateForWebContents(app_contents.get());
  extensions::TabHelper* extensions_tab_helper =
      extensions::TabHelper::FromWebContents(app_contents.get());
  extensions_tab_helper->SetExtensionApp(extension_app);

  model->AddWebContents(std::move(app_contents), 0,
                        ui::PageTransitionFromInt(0), AddTabTypes::ADD_NONE);
  model->SetTabPinned(0, true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  TabClosingObserver observer;
  model->AddObserver(&observer);

  // Uninstall the extension and make sure TabClosing is sent.
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(browser()->profile())
          ->extension_service();
  service->UninstallExtension(
      GetExtension()->id(), extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
  EXPECT_EQ(1, observer.closing_count());

  model->RemoveObserver(&observer);

  // There should only be one tab now.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
}

// Open with --app-id=<id>, and see that an application window opens by default.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(BrowserTest, AppIdSwitch) {
  base::HistogramTester tester;
  ASSERT_TRUE(embedded_test_server()->Start());

  // There should be one browser and one tab to start with.
  EXPECT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  // Load an app.
  webapps::AppId app_id = web_app::test::InstallDummyWebApp(
      browser()->profile(), "testapp", GURL("https://testapp.com"));
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, app_id);

  ui_test_utils::BrowserChangeObserver browser_change(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  base::test::TestFuture<void> launch_done;
  web_app::startup::SetStartupDoneCallbackForTesting(launch_done.GetCallback());
  EXPECT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), chrome::startup::IsProcessStartup::kNo,
      {browser()->profile(), StartupProfileMode::kBrowserWindow}, {}));

  ASSERT_TRUE(launch_done.Wait());
  Browser* app_browser = browser_change.Wait();
  EXPECT_TRUE(app_browser->is_type_app());

#if BUILDFLAG(IS_WIN)
  {  // From launch_mode_recorder.cc:
    constexpr char kLaunchModesHistogram[] = "Launch.Mode2";
    const base::HistogramBase::Sample kWebAppOther = 22;

    tester.ExpectUniqueSample(kLaunchModesHistogram, kWebAppOther, 1);
  }
#endif  // BUILDFLAG(IS_WIN)

  // Check that the number of browsers and tabs is correct.
  EXPECT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Overscroll is only enabled on Aura platforms currently, and even then only
// when a specific feature (OverscrollHistoryNavigation) is enabled.
#if defined(USE_AURA)
IN_PROC_BROWSER_TEST_F(BrowserTest, OverscrollEnabledInRegularWindows) {
  ASSERT_TRUE(browser()->is_type_normal());
  EXPECT_TRUE(browser()->CanOverscrollContent());
}

IN_PROC_BROWSER_TEST_F(BrowserTest, OverscrollEnabledInPopups) {
  Browser* popup_browser = Browser::Create(
      Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile(), true));
  ASSERT_TRUE(popup_browser->is_type_popup());
  EXPECT_TRUE(popup_browser->CanOverscrollContent());
}

IN_PROC_BROWSER_TEST_F(BrowserTest, OverscrollDisabledInDevToolsWindows) {
  DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), false);
  Browser* dev_tools_browser = chrome::FindLastActive();
  ASSERT_EQ(dev_tools_browser->app_name(), DevToolsWindow::kDevToolsApp);
  EXPECT_FALSE(dev_tools_browser->CanOverscrollContent());
}
#endif

// Open an app window and the dev tools window and ensure that the location
// bar settings are correct.
IN_PROC_BROWSER_TEST_F(BrowserTest, ShouldShowLocationBar) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load an app.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app/")));
  const Extension* extension_app = GetExtension();

  // Launch it in a window, as AppLauncherHandler::HandleLaunchApp() would.
  WebContents* app_window =
      apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
          ->BrowserAppLauncher()
          ->LaunchAppWithParamsForTesting(apps::AppLaunchParams(
              extension_app->id(),
              apps::LaunchContainer::kLaunchContainerWindow,
              WindowOpenDisposition::NEW_WINDOW,
              apps::LaunchSource::kFromTest));
  ASSERT_TRUE(app_window);

  DevToolsWindow* devtools_window =
      DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), false);

  // The launch should have created a new app browser and a dev tools browser.
  ASSERT_EQ(3u, chrome::GetBrowserCount(browser()->profile()));

  // Find the new browsers.
  Browser* app_browser = nullptr;
  Browser* dev_tools_browser = nullptr;
  for (Browser* b : *BrowserList::GetInstance()) {
    if (b == browser()) {
      continue;
    } else if (b->app_name() == DevToolsWindow::kDevToolsApp) {
      dev_tools_browser = b;
    } else {
      app_browser = b;
    }
  }
  ASSERT_TRUE(dev_tools_browser);
  ASSERT_TRUE(app_browser);
  ASSERT_TRUE(app_browser != browser());

  EXPECT_FALSE(
      dev_tools_browser->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR));

  // App windows can show location bars, for example when they navigate away
  // from their starting origin.
  EXPECT_TRUE(app_browser->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR));

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools_window);
}

// Regression test for crbug.com/702505.
IN_PROC_BROWSER_TEST_F(BrowserTest, ReattachDevToolsWindow) {
  ASSERT_TRUE(embedded_test_server()->Start());
  net::EmbeddedTestServer https_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(https_test_server.Start());
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), http_url));

  // Open a devtools window.
  DevToolsWindow* devtools_window =
      DevToolsWindowTesting::OpenDevToolsWindowSync(browser(),
                                                    /*is_docked=*/true);
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));

  // Grab its main web contents.
  content::WebContents* devtools_main_web_contents =
      DevToolsWindow::GetInTabWebContents(
          devtools_window->GetInspectedWebContents(), nullptr);
  ASSERT_NE(web_contents, devtools_main_web_contents);

  // Detach the devtools window.
  DevToolsUIBindings::Delegate* devtools_delegate =
      static_cast<DevToolsUIBindings::Delegate*>(devtools_window);
  devtools_delegate->SetIsDocked(false);
  // This should have created a new dev tools browser.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));

  // Re-attach the dev tools window. This resets its Browser*.
  devtools_delegate->SetIsDocked(true);
  // Wait until the browser actually gets closed.
  ui_test_utils::WaitForBrowserToClose();
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));

  // Do something that will make SearchTabHelper access its OmniboxView. This
  // should not crash, even though the Browser association and thus the
  // OmniboxView* has changed, and the old OmniboxView has been deleted.
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(devtools_main_web_contents);
  SearchIPCRouter::Delegate* search_ipc_router_delegate =
      static_cast<SearchIPCRouter::Delegate*>(search_tab_helper);
  search_ipc_router_delegate->FocusOmnibox(OMNIBOX_FOCUS_INVISIBLE);

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools_window);
}

// Chromeos defaults to restoring the last session, so this test isn't
// applicable.
#if !BUILDFLAG(IS_CHROMEOS)
// Makes sure pinned tabs are restored correctly on start.
IN_PROC_BROWSER_TEST_F(BrowserTest, RestorePinnedTabs) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Add a pinned tab.
  GURL url(embedded_test_server()->GetURL("/empty.html"));
  TabStripModel* model = browser()->tab_strip_model();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  model->SetTabPinned(0, true);

  // Add a non pinned tab.
  chrome::NewTab(browser());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Add another pinned tab.
  chrome::NewTab(browser());
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  model->SetTabPinned(2, true);

  // Write out the pinned tabs.
  PinnedTabCodec::WritePinnedTabs(browser()->profile());

  // Set last What's New version to the current version so there is no What's
  // New tab shown on launch (for the non-first-run case).
  g_browser_process->local_state()->SetInteger(prefs::kLastWhatsNewVersion,
                                               CHROME_VERSION_MAJOR);

  // Close the browser window.
  browser()->window()->Close();

  // Launch again with the same profile.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  chrome::startup::IsFirstRun first_run =
      first_run::IsChromeFirstRun() ? chrome::startup::IsFirstRun::kYes
                                    : chrome::startup::IsFirstRun::kNo;
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, first_run);
  launch.Launch(browser()->profile(), chrome::startup::IsProcessStartup::kNo,
                /*restore_tabbed_browser=*/true);

  // The launch should have created a new browser.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));

  // Find the new browser.
  BrowserList* browsers = BrowserList::GetInstance();
  auto new_browser_iter = base::ranges::find_if_not(
      *browsers, [this](Browser* b) { return b == browser(); });
  ASSERT_NE(browsers->end(), new_browser_iter);

  Browser* new_browser = *new_browser_iter;

  // We should get back an additional tab for the app, and another for the
  // default home page.
  ASSERT_EQ(3, new_browser->tab_strip_model()->count());

  // Make sure the state matches.
  TabStripModel* new_model = new_browser->tab_strip_model();
  EXPECT_TRUE(new_model->IsTabPinned(0));
  EXPECT_TRUE(new_model->IsTabPinned(1));
  EXPECT_FALSE(new_model->IsTabPinned(2));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

// TODO(crbug.com/40148102): fix the way how exo creates accelerated widgets. At
// the moment, they are created only after the client attaches a buffer to a
// surface, which is incorrect and results in the "[destroyed object]: error 1:
// popup parent not constructed" error.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_CloseWithAppMenuOpen DISABLED_CloseWithAppMenuOpen
#else
#define MAYBE_CloseWithAppMenuOpen CloseWithAppMenuOpen
#endif
// This test verifies we don't crash when closing the last window and the app
// menu is showing.
IN_PROC_BROWSER_TEST_F(BrowserTest, MAYBE_CloseWithAppMenuOpen) {
  if (browser_defaults::kBrowserAliveWithNoWindows)
    return;

  // We need a message loop running for menus on windows.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&RunCloseWithAppMenuCallback, browser()));
}

#if !BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(BrowserTest, OpenAppWindowLikeNtp) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load an app
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app/")));
  const Extension* extension_app = GetExtension();
  ASSERT_TRUE(extension_app);

  // Launch it in a window, as AppLauncherHandler::HandleLaunchApp() would.
  WebContents* app_window =
      apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
          ->BrowserAppLauncher()
          ->LaunchAppWithParamsForTesting(apps::AppLaunchParams(
              extension_app->id(),
              apps::LaunchContainer::kLaunchContainerWindow,
              WindowOpenDisposition::NEW_WINDOW,
              apps::LaunchSource::kFromTest));
  ASSERT_TRUE(app_window);

  // Apps launched in a window from the NTP have an extensions tab helper with
  // extension_app set.
  ASSERT_TRUE(extensions::TabHelper::FromWebContents(app_window));
  EXPECT_TRUE(extensions::TabHelper::FromWebContents(app_window)->is_app());
  EXPECT_EQ(extensions::AppLaunchInfo::GetFullLaunchURL(extension_app),
            app_window->GetURL());

  // The launch should have created a new browser.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));

  // Find the new browser.
  Browser* new_browser = nullptr;
  for (Browser* b : *BrowserList::GetInstance()) {
    if (b != browser())
      new_browser = b;
  }
  ASSERT_TRUE(new_browser);
  ASSERT_TRUE(new_browser != browser());

  EXPECT_TRUE(new_browser->is_type_app());

  // The browser's app name should include the extension's id.
  std::string app_name = new_browser->app_name_;
  EXPECT_NE(app_name.find(extension_app->id()), std::string::npos)
      << "Name " << app_name << " should contain id " << extension_app->id();
}
#endif  // !BUILDFLAG(IS_MAC)

// Makes sure the browser doesn't crash when
// set_show_state(ui::mojom::WindowShowState::kMaximized) has been invoked.
IN_PROC_BROWSER_TEST_F(BrowserTest, StartMaximized) {
  Browser::CreateParams params[] = {
      Browser::CreateParams(Browser::TYPE_NORMAL, browser()->profile(), true),
      Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile(), true),
      Browser::CreateParams::CreateForApp("app_name", true, gfx::Rect(),
                                          browser()->profile(), true),
      Browser::CreateParams::CreateForDevTools(browser()->profile()),
      Browser::CreateParams::CreateForAppPopup("app_name", true, gfx::Rect(),
                                               browser()->profile(), true),
      Browser::CreateParams(Browser::TYPE_PICTURE_IN_PICTURE,
                            browser()->profile(), true),
  };
  for (size_t i = 0; i < std::size(params); ++i) {
    params[i].initial_show_state = ui::mojom::WindowShowState::kMaximized;
    AddBlankTabAndShow(Browser::Create(params[i]));
  }
}

// TODO(crbug.com/40248487) This test is flaky on asan lacros and may crash ash.
#if BUILDFLAG(IS_CHROMEOS_LACROS) && defined(ADDRESS_SANITIZER)
#define MAYBE_StartMinimized DISABLED_StartMinimized
#else
#define MAYBE_StartMinimized StartMinimized
#endif
// Makes sure the browser doesn't crash when
// set_show_state(ui::mojom::WindowShowState::kMinimized) has been invoked.
IN_PROC_BROWSER_TEST_F(BrowserTest, MAYBE_StartMinimized) {
  Browser::CreateParams params[] = {
      Browser::CreateParams(Browser::TYPE_NORMAL, browser()->profile(), true),
      Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile(), true),
      Browser::CreateParams::CreateForApp("app_name", true, gfx::Rect(),
                                          browser()->profile(), true),
      Browser::CreateParams::CreateForDevTools(browser()->profile()),
      Browser::CreateParams::CreateForAppPopup("app_name", true, gfx::Rect(),
                                               browser()->profile(), true),
      Browser::CreateParams(Browser::TYPE_PICTURE_IN_PICTURE,
                            browser()->profile(), true),
  };
  for (size_t i = 0; i < std::size(params); ++i) {
    params[i].initial_show_state = ui::mojom::WindowShowState::kMinimized;
    AddBlankTabAndShow(Browser::Create(params[i]));
  }
}

// Makes sure the forward button is disabled immediately when navigating
// forward to a slow-to-commit page.
IN_PROC_BROWSER_TEST_F(BrowserTest, ForwardDisabledOnForward) {
  GURL blank_url(url::kAboutBlankURL);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), blank_url));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), ui_test_utils::GetTestUrl(
                     base::FilePath(base::FilePath::kCurrentDirectory),
                     base::FilePath(kTitle1File))));

  content::LoadStopObserver back_nav_load_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  back_nav_load_observer.Wait();
  CommandUpdater* command_updater = browser()->command_controller();
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_FORWARD));

  content::LoadStopObserver forward_nav_load_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  chrome::GoForward(browser(), WindowOpenDisposition::CURRENT_TAB);
  // This check will happen before the navigation completes, since the browser
  // won't process the renderer's response until the Wait() call below.
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_FORWARD));
  forward_nav_load_observer.Wait();
}

// Makes sure certain commands are disabled when Incognito mode is forced.
IN_PROC_BROWSER_TEST_F(BrowserTest, DisableMenuItemsWhenIncognitoIsForced) {
  CommandUpdater* command_updater = browser()->command_controller();
  // At the beginning, all commands are enabled.
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_NEW_WINDOW));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_NEW_INCOGNITO_WINDOW));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SHOW_BOOKMARK_MANAGER));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_IMPORT_SETTINGS));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_MANAGE_EXTENSIONS));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_OPTIONS));

  // Set Incognito to FORCED.
  IncognitoModePrefs::SetAvailability(
      browser()->profile()->GetPrefs(),
      policy::IncognitoModeAvailability::kForced);
  // Bookmarks & Settings commands should get disabled.
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_NEW_WINDOW));
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_SHOW_BOOKMARK_MANAGER));
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_IMPORT_SETTINGS));
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_MANAGE_EXTENSIONS));
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_OPTIONS));
  // New Incognito Window command, however, should be enabled.
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_NEW_INCOGNITO_WINDOW));

  // Create a new browser.
  Browser* new_browser = Browser::Create(Browser::CreateParams(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
      true));
  CommandUpdater* new_command_updater = new_browser->command_controller();
  // It should have Bookmarks & Settings commands disabled by default.
  EXPECT_FALSE(new_command_updater->IsCommandEnabled(IDC_NEW_WINDOW));
  EXPECT_FALSE(
      new_command_updater->IsCommandEnabled(IDC_SHOW_BOOKMARK_MANAGER));
  EXPECT_FALSE(new_command_updater->IsCommandEnabled(IDC_IMPORT_SETTINGS));
  EXPECT_FALSE(new_command_updater->IsCommandEnabled(IDC_MANAGE_EXTENSIONS));
  EXPECT_FALSE(new_command_updater->IsCommandEnabled(IDC_OPTIONS));
  EXPECT_TRUE(new_command_updater->IsCommandEnabled(IDC_NEW_INCOGNITO_WINDOW));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(BrowserTest, ArcBrowserWindowFeaturesSetCorrectly) {
  Browser* new_browser = Browser::Create(
      Browser::CreateParams(Browser::TYPE_CUSTOM_TAB, browser()->profile(),
                            /* user_gesture= */ true));
  ASSERT_TRUE(new_browser);

  EXPECT_FALSE(new_browser->SupportsWindowFeature(
      Browser::WindowFeature::FEATURE_LOCATIONBAR));
  EXPECT_FALSE(new_browser->SupportsWindowFeature(
      Browser::WindowFeature::FEATURE_TITLEBAR));
  EXPECT_FALSE(new_browser->SupportsWindowFeature(
      Browser::WindowFeature::FEATURE_TABSTRIP));
  EXPECT_FALSE(new_browser->SupportsWindowFeature(
      Browser::WindowFeature::FEATURE_BOOKMARKBAR));
  EXPECT_FALSE(
      new_browser->SupportsWindowFeature(Browser::WindowFeature::FEATURE_NONE));

  EXPECT_TRUE(new_browser->SupportsWindowFeature(
      Browser::WindowFeature::FEATURE_TOOLBAR));
}
#endif

// Makes sure New Incognito Window command is disabled when Incognito mode is
// not available.
IN_PROC_BROWSER_TEST_F(BrowserTest,
                       NoNewIncognitoWindowWhenIncognitoIsDisabled) {
  CommandUpdater* command_updater = browser()->command_controller();
  // Set Incognito to DISABLED.
  IncognitoModePrefs::SetAvailability(
      browser()->profile()->GetPrefs(),
      policy::IncognitoModeAvailability::kDisabled);
  // Make sure New Incognito Window command is disabled. All remaining commands
  // should be enabled.
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_NEW_INCOGNITO_WINDOW));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_NEW_WINDOW));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SHOW_BOOKMARK_MANAGER));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_IMPORT_SETTINGS));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_MANAGE_EXTENSIONS));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_OPTIONS));

  // Create a new browser.
  Browser* new_browser =
      Browser::Create(Browser::CreateParams(browser()->profile(), true));
  CommandUpdater* new_command_updater = new_browser->command_controller();
  EXPECT_FALSE(new_command_updater->IsCommandEnabled(IDC_NEW_INCOGNITO_WINDOW));
  EXPECT_TRUE(new_command_updater->IsCommandEnabled(IDC_NEW_WINDOW));
  EXPECT_TRUE(new_command_updater->IsCommandEnabled(IDC_SHOW_BOOKMARK_MANAGER));
  EXPECT_TRUE(new_command_updater->IsCommandEnabled(IDC_IMPORT_SETTINGS));
  EXPECT_TRUE(new_command_updater->IsCommandEnabled(IDC_MANAGE_EXTENSIONS));
  EXPECT_TRUE(new_command_updater->IsCommandEnabled(IDC_OPTIONS));
}

class BrowserTestWithExtensionsDisabled : public BrowserTest {
 protected:
  BrowserTestWithExtensionsDisabled() = default;

 public:
  BrowserTestWithExtensionsDisabled(const BrowserTestWithExtensionsDisabled&) =
      delete;
  BrowserTestWithExtensionsDisabled& operator=(
      const BrowserTestWithExtensionsDisabled&) = delete;

 protected:
  ~BrowserTestWithExtensionsDisabled() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    BrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableExtensions);
  }
};

// Makes sure Extensions and Settings commands are disabled in certain
// circumstances even though normally they should stay enabled.
IN_PROC_BROWSER_TEST_F(BrowserTestWithExtensionsDisabled,
                       DisableExtensionsAndSettingsWhenIncognitoIsDisabled) {
  CommandUpdater* command_updater = browser()->command_controller();
  // Set Incognito to DISABLED.
  IncognitoModePrefs::SetAvailability(
      browser()->profile()->GetPrefs(),
      policy::IncognitoModeAvailability::kDisabled);
  // Make sure Manage Extensions command is disabled.
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_MANAGE_EXTENSIONS));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_NEW_WINDOW));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SHOW_BOOKMARK_MANAGER));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_IMPORT_SETTINGS));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_OPTIONS));

  // Create a popup (non-main-UI-type) browser. Settings command as well
  // as Extensions should be disabled.
  Browser* popup_browser = Browser::Create(
      Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile(), true));
  CommandUpdater* popup_command_updater = popup_browser->command_controller();
  EXPECT_FALSE(popup_command_updater->IsCommandEnabled(IDC_MANAGE_EXTENSIONS));
  EXPECT_FALSE(popup_command_updater->IsCommandEnabled(IDC_OPTIONS));
  EXPECT_TRUE(
      popup_command_updater->IsCommandEnabled(IDC_SHOW_BOOKMARK_MANAGER));
  EXPECT_FALSE(popup_command_updater->IsCommandEnabled(IDC_IMPORT_SETTINGS));
}

// Makes sure Extensions and Settings commands are disabled in certain
// circumstances even though normally they should stay enabled.
IN_PROC_BROWSER_TEST_F(BrowserTest,
                       DisableOptionsAndImportMenuItemsConsistently) {
  // Create a popup browser.
  Browser* popup_browser = Browser::Create(
      Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile(), true));
  CommandUpdater* command_updater = popup_browser->command_controller();
  // OPTIONS and IMPORT_SETTINGS are disabled for a non-normal UI.
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_OPTIONS));
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_IMPORT_SETTINGS));

  // Set Incognito to FORCED.
  IncognitoModePrefs::SetAvailability(
      popup_browser->profile()->GetPrefs(),
      policy::IncognitoModeAvailability::kForced);
  // OPTIONS and IMPORT_SETTINGS are disabled when Incognito is forced.
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_OPTIONS));
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_IMPORT_SETTINGS));
  // Set Incognito to AVAILABLE.
  IncognitoModePrefs::SetAvailability(
      popup_browser->profile()->GetPrefs(),
      policy::IncognitoModeAvailability::kEnabled);
  // OPTIONS and IMPORT_SETTINGS are still disabled since it is a non-normal UI.
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_OPTIONS));
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_IMPORT_SETTINGS));
}

namespace {

void OnZoomLevelChanged(base::OnceClosure* callback,
                        const HostZoomMap::ZoomLevelChange& host) {
  std::move(*callback).Run();
}

int GetZoomPercent(content::WebContents* contents,
                   bool* enable_plus,
                   bool* enable_minus) {
  int percent =
      zoom::ZoomController::FromWebContents(contents)->GetZoomPercent();
  *enable_plus = percent < contents->GetMaximumZoomPercent();
  *enable_minus = percent > contents->GetMinimumZoomPercent();
  return percent;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(BrowserTest, PageZoom) {
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  bool enable_plus, enable_minus;

  {
    scoped_refptr<content::MessageLoopRunner> loop_runner(
        new content::MessageLoopRunner);
    base::OnceClosure quit_closure = loop_runner->QuitClosure();
    content::HostZoomMap::ZoomLevelChangedCallback callback =
        base::BindRepeating(&OnZoomLevelChanged, &quit_closure);
    base::CallbackListSubscription subscription =
        content::HostZoomMap::GetDefaultForBrowserContext(browser()->profile())
            ->AddZoomLevelChangedCallback(std::move(callback));
    chrome::Zoom(browser(), content::PAGE_ZOOM_IN);
    loop_runner->Run();
    subscription = {};
    EXPECT_EQ(GetZoomPercent(contents, &enable_plus, &enable_minus), 110);
    EXPECT_TRUE(enable_plus);
    EXPECT_TRUE(enable_minus);
  }

  {
    scoped_refptr<content::MessageLoopRunner> loop_runner(
        new content::MessageLoopRunner);
    base::OnceClosure quit_closure = loop_runner->QuitClosure();
    content::HostZoomMap::ZoomLevelChangedCallback callback =
        base::BindRepeating(&OnZoomLevelChanged, &quit_closure);
    base::CallbackListSubscription subscription =
        content::HostZoomMap::GetDefaultForBrowserContext(browser()->profile())
            ->AddZoomLevelChangedCallback(std::move(callback));
    chrome::Zoom(browser(), content::PAGE_ZOOM_RESET);
    loop_runner->Run();
    subscription = {};
    EXPECT_EQ(GetZoomPercent(contents, &enable_plus, &enable_minus), 100);
    EXPECT_TRUE(enable_plus);
    EXPECT_TRUE(enable_minus);
  }

  {
    scoped_refptr<content::MessageLoopRunner> loop_runner(
        new content::MessageLoopRunner);
    base::OnceClosure quit_closure = loop_runner->QuitClosure();
    content::HostZoomMap::ZoomLevelChangedCallback callback =
        base::BindRepeating(&OnZoomLevelChanged, &quit_closure);
    base::CallbackListSubscription subscription =
        content::HostZoomMap::GetDefaultForBrowserContext(browser()->profile())
            ->AddZoomLevelChangedCallback(std::move(callback));
    chrome::Zoom(browser(), content::PAGE_ZOOM_OUT);
    loop_runner->Run();
    subscription = {};
    EXPECT_EQ(GetZoomPercent(contents, &enable_plus, &enable_minus), 90);
    EXPECT_TRUE(enable_plus);
    EXPECT_TRUE(enable_minus);
  }

  chrome::Zoom(browser(), content::PAGE_ZOOM_RESET);
}

// TODO(ben): this test was never enabled. It has bit-rotted since being added.
// It originally lived in browser_unittest.cc, but has been moved here to make
// room for real browser unit tests.
#if 0
class BrowserTest2 : public InProcessBrowserTest {
 public:
  BrowserTest2() {
    host_resolver_proc_ = new net::RuleBasedHostResolverProc(NULL);
    // Avoid making external DNS lookups. In this test we don't need this
    // to succeed.
    host_resolver_proc_->AddSimulatedFailure("*.google.com");
    scoped_host_resolver_proc_.Init(host_resolver_proc_.get());
  }

 private:
  scoped_refptr<net::RuleBasedHostResolverProc> host_resolver_proc_;
  net::ScopedDefaultHostResolverProc scoped_host_resolver_proc_;
};

IN_PROC_BROWSER_TEST_F(BrowserTest2, NoTabsInPopups) {
  chrome::RegisterAppPrefs(L"Test");

  // We start with a normal browser with one tab.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Open a popup browser with a single blank foreground tab.
  Browser* popup_browser = Browser::Create(
      Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile()));
  chrome::AddTabAt(popup_browser, GURL(), -1, true);
  EXPECT_EQ(1, popup_browser->tab_strip_model()->count());

  // Now try opening another tab in the popup browser.
  AddTabWithURLParams params1(url, ui::PAGE_TRANSITION_TYPED);
  popup_browser->AddTabWithURL(&params1);
  EXPECT_EQ(popup_browser, params1.target);

  // The popup should still only have one tab.
  EXPECT_EQ(1, popup_browser->tab_strip_model()->count());

  // The normal browser should now have two.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  // Open an app frame browser with a single blank foreground tab.
  Browser* app_browser = Browser::Create(Browser::CreateParams::CreateForApp(
      L"Test", browser()->profile(), false));
  chrome::AddTabAt(app_browser, GURL(), -1, true);
  EXPECT_EQ(1, app_browser->tab_strip_model()->count());

  // Now try opening another tab in the app browser.
  AddTabWithURLParams params2(GURL(url::kAboutBlankURL),
                              ui::PAGE_TRANSITION_TYPED);
  app_browser->AddTabWithURL(&params2);
  EXPECT_EQ(app_browser, params2.target);

  // The popup should still only have one tab.
  EXPECT_EQ(1, app_browser->tab_strip_model()->count());

  // The normal browser should now have three.
  EXPECT_EQ(3, browser()->tab_strip_model()->count());

  // Open an app frame popup browser with a single blank foreground tab.
  Browser* app_popup_browser = Browser::Create(
      Browser::CreateParams::CreateForApp(
          L"Test", browser()->profile(), false));
  chrome::AddTabAt(app_popup_browser, GURL(), -1, true);
  EXPECT_EQ(1, app_popup_browser->tab_strip_model()->count());

  // Now try opening another tab in the app popup browser.
  AddTabWithURLParams params3(GURL(url::kAboutBlankURL),
                              ui::PAGE_TRANSITION_TYPED);
  app_popup_browser->AddTabWithURL(&params3);
  EXPECT_EQ(app_popup_browser, params3.target);

  // The popup should still only have one tab.
  EXPECT_EQ(1, app_popup_browser->tab_strip_model()->count());

  // The normal browser should now have four.
  EXPECT_EQ(4, browser()->tab_strip_model()->count());

  // Close the additional browsers.
  popup_browser->tab_strip_model()->CloseAllTabs();
  app_browser->tab_strip_model()->CloseAllTabs();
  app_popup_browser->tab_strip_model()->CloseAllTabs();
}
#endif

IN_PROC_BROWSER_TEST_F(BrowserTest, WindowOpenClose1) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      embedder_support::kDisablePopupBlocking);
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/window.close.html");
  GURL::Replacements add_query;
  std::string query("test1");
  add_query.SetQueryStr(query);
  url = url.ReplaceComponents(add_query);

  std::u16string title = u"Title Of Awesomeness";
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), title);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url, 2);
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(BrowserTest, WindowOpenClose2) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      embedder_support::kDisablePopupBlocking);
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/window.close.html");
  GURL::Replacements add_query;
  std::string query("test2");
  add_query.SetQueryStr(query);
  url = url.ReplaceComponents(add_query);

  std::u16string title = u"Title Of Awesomeness";
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), title);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url, 2);
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
}

// Disabled because of timeouts in several builders.
// https://crbug.com/1129313
IN_PROC_BROWSER_TEST_F(BrowserTest, DISABLED_WindowOpenClose3) {
#if BUILDFLAG(IS_MAC)
  // Ensure that tests don't wait for frames that will never come.
  ui::CATransactionCoordinator::Get().DisableForTesting();
#endif
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      embedder_support::kDisablePopupBlocking);
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/window.close.html");
  GURL::Replacements add_query;
  std::string query("test3");
  add_query.SetQueryStr(query);
  url = url.ReplaceComponents(add_query);

  std::u16string title = u"Title Of Awesomeness";
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), title);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url, 2);
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
}

// TODO(linux_aura) http://crbug.com/163931
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if !(BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
IN_PROC_BROWSER_TEST_F(BrowserTest, FullscreenBookmarkBar) {
#if BUILDFLAG(IS_MAC)
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;
#endif

  chrome::ToggleBookmarkBar(browser());
  EXPECT_EQ(BookmarkBar::SHOW, browser()->bookmark_bar_state());
  chrome::ToggleFullscreenMode(browser());
  EXPECT_TRUE(browser()->window()->IsFullscreen());
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH)
  // Mac and Chrome OS both have an "immersive style" fullscreen where the
  // bookmark bar is visible when the top views slide down.
  EXPECT_EQ(BookmarkBar::SHOW, browser()->bookmark_bar_state());
#else
  EXPECT_EQ(BookmarkBar::HIDDEN, browser()->bookmark_bar_state());
#endif
}
#endif

IN_PROC_BROWSER_TEST_F(BrowserTest, DisallowFileUrlUniversalAccessTest) {
  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(),
      base::FilePath().AppendASCII("fileurl_universalaccess.html"));

  std::u16string expected_title(u"Disallowed");
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
  title_watcher.AlsoWaitForTitle(u"Allowed");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

class KioskModeTest : public BrowserTest {
 public:
  KioskModeTest() = default;

  void SetUpOnMainThread() override {
    BrowserTest::SetUpOnMainThread();
    browser()->window()->SetForceFullscreen(true);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kKioskMode);
  }
};

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
// Mac: http://crbug.com/103912
// Linux: http://crbug.com/163931
#define MAYBE_EnableKioskModeTest DISABLED_EnableKioskModeTest
#else
#define MAYBE_EnableKioskModeTest EnableKioskModeTest
#endif
IN_PROC_BROWSER_TEST_F(KioskModeTest, MAYBE_EnableKioskModeTest) {
  // Check if browser is in fullscreen mode.
  ASSERT_TRUE(browser()->window()->IsFullscreen());
  ASSERT_FALSE(browser()->window()->IsFullscreenBubbleVisible());
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(KioskModeTest, DoNotExitFullscreen) {
  browser()->window()->GetExclusiveAccessContext()->ExitFullscreen();
  ASSERT_TRUE(browser()->window()->IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(KioskModeTest, DoNotChangeBounds) {
  gfx::Rect old_bounds = browser()->window()->GetBounds();

  browser()->window()->SetBounds(gfx::Rect(10, 10, 10, 10));
  gfx::Rect new_bounds = browser()->window()->GetBounds();

  ASSERT_TRUE(browser()->window()->IsFullscreen());
  ASSERT_EQ(old_bounds, new_bounds);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
// This test verifies that Chrome can be launched with a user-data-dir path
// which contains non ASCII characters.
class LaunchBrowserWithNonAsciiUserDatadir : public BrowserTest {
 public:
  LaunchBrowserWithNonAsciiUserDatadir() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath tmp_profile = temp_dir_.GetPath().AppendASCII("tmp_profile");
    tmp_profile = tmp_profile.Append(L"Test Chrome G\u00E9raldine");

    ASSERT_TRUE(base::CreateDirectory(tmp_profile));
    command_line->AppendSwitchPath(switches::kUserDataDir, tmp_profile);
  }

  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(LaunchBrowserWithNonAsciiUserDatadir,
                       TestNonAsciiUserDataDir) {
  // Verify that the window is present.
  ASSERT_TRUE(browser());
  ASSERT_TRUE(browser()->profile());
  // Verify that the profile has been added correctly to the
  // ProfileAttributesStorage.
  ASSERT_EQ(1u, g_browser_process->profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetNumberOfProfiles());
}

// This test verifies that Chrome can be launched with a user-data-dir path
// which contains a reparse point. This is important because sandbox
// policy validates that paths passed to policy rules do not contain
// reparse points. New code in Chrome that adjusts the sandbox can
// accidentally pass paths with reparse points to the sandbox and cause
// Chrome not to start anymore.
class LaunchBrowserWithReparsePointUserDatadir : public BrowserTest {
 public:
  LaunchBrowserWithReparsePointUserDatadir() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath tmp_profile = temp_dir_.GetPath().AppendASCII("profile");
    ASSERT_TRUE(base::CreateDirectory(tmp_profile));
    base::FilePath reparse_profile =
        temp_dir_.GetPath().AppendASCII("profile_reparse");
    ASSERT_TRUE(base::CreateDirectory(reparse_profile));
    auto reparse_point =
        base::test::FilePathReparsePoint::Create(reparse_profile, tmp_profile);
    ASSERT_TRUE(reparse_point.has_value());
    reparse_point_.emplace(std::move(reparse_point.value()));
    command_line->AppendSwitchPath(switches::kUserDataDir, reparse_profile);
  }

  base::ScopedTempDir temp_dir_;
  std::optional<base::test::FilePathReparsePoint> reparse_point_;
};

IN_PROC_BROWSER_TEST_F(LaunchBrowserWithReparsePointUserDatadir,
                       TestReparsePointUserDataDir) {
  // Verify that the window is present.
  ASSERT_TRUE(browser());
  ASSERT_TRUE(browser()->profile());
  // Verify that the profile has been added correctly to the
  // ProfileAttributesStorage.
  ASSERT_EQ(1u, g_browser_process->profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetNumberOfProfiles());
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN)
// This test verifies that Chrome can be launched with a user-data-dir path
// which trailing slashes.
class LaunchBrowserWithTrailingSlashDatadir : public BrowserTest {
 public:
  LaunchBrowserWithTrailingSlashDatadir() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath tmp_profile = temp_dir_.GetPath().AppendASCII("tmp_profile");
    tmp_profile = tmp_profile.Append(L"Test Chrome\\");

    ASSERT_TRUE(base::CreateDirectory(tmp_profile));
    command_line->AppendSwitchPath(switches::kUserDataDir, tmp_profile);
  }

  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(LaunchBrowserWithTrailingSlashDatadir,
                       TestTrailingSlashUserDataDir) {
  // Verify that the window is present.
  ASSERT_TRUE(browser());
  ASSERT_TRUE(browser()->profile());
  // Verify that the profile has been added correctly to the
  // ProfileAttributesStorage.
  ASSERT_EQ(1u, g_browser_process->profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetNumberOfProfiles());
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
// Tests to ensure that the browser continues running in the background after
// the last window closes.
class RunInBackgroundTest : public BrowserTest {
 public:
  RunInBackgroundTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kKeepAliveForTest);
  }
};

IN_PROC_BROWSER_TEST_F(RunInBackgroundTest, RunInBackgroundBasicTest) {
  // Close the browser window, then open a new one - the browser should keep
  // running.
  Profile* profile = browser()->profile();
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  CloseBrowserSynchronously(browser());
  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());

  chrome::NewEmptyWindow(profile);

  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
}
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)

// Tests to ensure that the browser continues running in the background after
// the last window closes.
class NoStartupWindowTest : public BrowserTest {
 public:
  NoStartupWindowTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kNoStartupWindow);
    command_line->AppendSwitch(switches::kKeepAliveForTest);
  }

  // Returns true if any commands were processed.
  bool ProcessedAnyCommands(
      sessions::CommandStorageManager* command_storage_manager) {
    sessions::CommandStorageManagerTestHelper test_helper(
        command_storage_manager);
    return test_helper.ProcessedAnyCommands();
  }
};

IN_PROC_BROWSER_TEST_F(NoStartupWindowTest, NoStartupWindowBasicTest) {
  // No browser window should be started by default.
  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());

  // Starting a browser window should work just fine.
  CreateBrowser(ProfileManager::GetLastUsedProfile());

  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
}

// Chromeos needs to track app windows because it considers them to be part of
// session state.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(NoStartupWindowTest, DontInitSessionServiceForApps) {
  Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();

  SessionService* session_service =
      SessionServiceFactory::GetForProfile(profile);
  sessions::CommandStorageManager* command_storage_manager =
      session_service->GetCommandStorageManagerForTest();
  ASSERT_FALSE(ProcessedAnyCommands(command_storage_manager));

  CreateBrowserForApp("blah", profile);

  ASSERT_FALSE(ProcessedAnyCommands(command_storage_manager));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// This test needs to be placed outside the anonymous namespace because we
// need to access private type of Browser.
class AppModeTest : public BrowserTest {
 public:
  AppModeTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    GURL url = ui_test_utils::GetTestUrl(
        base::FilePath(), base::FilePath().AppendASCII("title1.html"));
    command_line->AppendSwitchASCII(switches::kApp, url.spec());
  }
};

IN_PROC_BROWSER_TEST_F(AppModeTest, EnableAppModeTest) {
  // Test that an application browser window loads correctly.

  // Verify the browser is in application mode.
  EXPECT_TRUE(browser()->is_type_app());
}

// Confirm chrome://version contains some expected content.
IN_PROC_BROWSER_TEST_F(BrowserTest, AboutVersion) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIVersionURL)));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_GT(
      ui_test_utils::FindInPage(tab, u"WebKit", true, true, nullptr, nullptr),
      0);
  ASSERT_GT(ui_test_utils::FindInPage(tab, u"OS", true, true, nullptr, nullptr),
            0);
  ASSERT_GT(ui_test_utils::FindInPage(tab, u"JavaScript", true, true, nullptr,
                                      nullptr),
            0);
}

static const base::FilePath::CharType* kTestDir =
    FILE_PATH_LITERAL("click_modifier");
static const char16_t kFirstPageTitle[] = u"First window";
static const char16_t kSecondPageTitle[] = u"New window!";

class ClickModifierTest : public InProcessBrowserTest {
 public:
  ClickModifierTest() = default;
  ClickModifierTest(const ClickModifierTest&) = delete;
  ClickModifierTest& operator=(const ClickModifierTest&) = delete;

  // Returns a url that opens a new window or tab when clicked, via javascript.
  GURL GetWindowOpenURL() const {
    return ui_test_utils::GetTestUrl(
        base::FilePath(kTestDir),
        base::FilePath(FILE_PATH_LITERAL("window_open.html")));
  }

  // Returns a url that follows a simple link when clicked, unless affected by
  // modifiers.
  GURL GetHrefURL() const {
    return ui_test_utils::GetTestUrl(
        base::FilePath(kTestDir),
        base::FilePath(FILE_PATH_LITERAL("href.html")));
  }

  std::u16string GetFirstPageTitle() const { return kFirstPageTitle; }

  std::u16string GetSecondPageTitle() const { return kSecondPageTitle; }

  // Loads our test page and simulates a single click using the supplied button
  // and modifiers.  The click will cause either a navigation or the creation of
  // a new window or foreground or background tab.  We verify that the expected
  // disposition occurs.
  void RunTest(Browser* browser,
               const GURL& url,
               int modifiers,
               blink::WebMouseEvent::Button button,
               WindowOpenDisposition disposition) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, url));
    EXPECT_EQ(1u, chrome::GetBrowserCount(browser->profile()));
    EXPECT_EQ(1, browser->tab_strip_model()->count());
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(url, web_contents->GetURL());

    if (disposition == WindowOpenDisposition::CURRENT_TAB) {
      content::TestNavigationObserver same_tab_observer(web_contents);
      SimulateMouseClick(web_contents, modifiers, button);
      same_tab_observer.Wait();
      EXPECT_EQ(1u, chrome::GetBrowserCount(browser->profile()));
      EXPECT_EQ(1, browser->tab_strip_model()->count());
      EXPECT_EQ(GetSecondPageTitle(), web_contents->GetTitle());
      return;
    }

    content::TestNavigationObserver new_tab_observer(nullptr);
    new_tab_observer.StartWatchingNewWebContents();
    SimulateMouseClick(web_contents, modifiers, button);
    new_tab_observer.Wait();

    if (disposition == WindowOpenDisposition::NEW_WINDOW) {
      EXPECT_EQ(2u, chrome::GetBrowserCount(browser->profile()));
      return;
    }

    EXPECT_EQ(1u, chrome::GetBrowserCount(browser->profile()));
    EXPECT_EQ(2, browser->tab_strip_model()->count());
    web_contents = browser->tab_strip_model()->GetActiveWebContents();
    if (disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB) {
      EXPECT_EQ(GetSecondPageTitle(), web_contents->GetTitle());
    } else {
      ASSERT_EQ(WindowOpenDisposition::NEW_BACKGROUND_TAB, disposition);
      EXPECT_EQ(GetFirstPageTitle(), web_contents->GetTitle());
    }
  }
};

// Tests for clicking on elements with handlers that run window.open.

IN_PROC_BROWSER_TEST_F(ClickModifierTest, WindowOpenBasicClickTest) {
  int modifiers = 0;
  blink::WebMouseEvent::Button button = blink::WebMouseEvent::Button::kLeft;
  WindowOpenDisposition disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  RunTest(browser(), GetWindowOpenURL(), modifiers, button, disposition);
}

// TODO(ericu): Alt-click behavior on window.open is platform-dependent and not
// well defined.  Should we add tests so we know if it changes?

// Shift-clicks open in a new window.
IN_PROC_BROWSER_TEST_F(ClickModifierTest, WindowOpenShiftClickTest) {
  int modifiers = blink::WebInputEvent::kShiftKey;
  blink::WebMouseEvent::Button button = blink::WebMouseEvent::Button::kLeft;
  WindowOpenDisposition disposition = WindowOpenDisposition::NEW_WINDOW;
  RunTest(browser(), GetWindowOpenURL(), modifiers, button, disposition);
}

// Control-clicks open in a background tab.
// On OSX meta [the command key] takes the place of control.
IN_PROC_BROWSER_TEST_F(ClickModifierTest, WindowOpenControlClickTest) {
#if BUILDFLAG(IS_MAC)
  int modifiers = blink::WebInputEvent::kMetaKey;
#else
  int modifiers = blink::WebInputEvent::kControlKey;
#endif
  blink::WebMouseEvent::Button button = blink::WebMouseEvent::Button::kLeft;
  WindowOpenDisposition disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  RunTest(browser(), GetWindowOpenURL(), modifiers, button, disposition);
}

// Control-shift-clicks open in a foreground tab.
// On OSX meta [the command key] takes the place of control.
IN_PROC_BROWSER_TEST_F(ClickModifierTest, WindowOpenControlShiftClickTest) {
#if BUILDFLAG(IS_MAC)
  int modifiers = blink::WebInputEvent::kMetaKey;
#else
  int modifiers = blink::WebInputEvent::kControlKey;
#endif
  modifiers |= blink::WebInputEvent::kShiftKey;
  blink::WebMouseEvent::Button button = blink::WebMouseEvent::Button::kLeft;
  WindowOpenDisposition disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  RunTest(browser(), GetWindowOpenURL(), modifiers, button, disposition);
}

// Tests for clicking on normal links.

IN_PROC_BROWSER_TEST_F(ClickModifierTest, HrefBasicClickTest) {
  int modifiers = 0;
  blink::WebMouseEvent::Button button = blink::WebMouseEvent::Button::kLeft;
  WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB;
  RunTest(browser(), GetHrefURL(), modifiers, button, disposition);
}

// TODO(ericu): Alt-click behavior on links is platform-dependent and not well
// defined.  Should we add tests so we know if it changes?

// Shift-clicks open in a new window.
IN_PROC_BROWSER_TEST_F(ClickModifierTest, HrefShiftClickTest) {
  int modifiers = blink::WebInputEvent::kShiftKey;
  blink::WebMouseEvent::Button button = blink::WebMouseEvent::Button::kLeft;
  WindowOpenDisposition disposition = WindowOpenDisposition::NEW_WINDOW;
  RunTest(browser(), GetHrefURL(), modifiers, button, disposition);
}

// Control-clicks open in a background tab.
// On OSX meta [the command key] takes the place of control.
IN_PROC_BROWSER_TEST_F(ClickModifierTest, HrefControlClickTest) {
#if BUILDFLAG(IS_MAC)
  int modifiers = blink::WebInputEvent::kMetaKey;
#else
  int modifiers = blink::WebInputEvent::kControlKey;
#endif
  blink::WebMouseEvent::Button button = blink::WebMouseEvent::Button::kLeft;
  WindowOpenDisposition disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  RunTest(browser(), GetHrefURL(), modifiers, button, disposition);
}

// Control-shift-clicks open in a foreground tab.
// On OSX meta [the command key] takes the place of control.
IN_PROC_BROWSER_TEST_F(ClickModifierTest, HrefControlShiftClickTest) {
#if BUILDFLAG(IS_MAC)
  int modifiers = blink::WebInputEvent::kMetaKey;
#else
  int modifiers = blink::WebInputEvent::kControlKey;
#endif
  modifiers |= blink::WebInputEvent::kShiftKey;
  blink::WebMouseEvent::Button button = blink::WebMouseEvent::Button::kLeft;
  WindowOpenDisposition disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  RunTest(browser(), GetHrefURL(), modifiers, button, disposition);
}

// Middle-clicks open in a background tab.
IN_PROC_BROWSER_TEST_F(ClickModifierTest, HrefMiddleClickTest) {
  int modifiers = 0;
  blink::WebMouseEvent::Button button = blink::WebMouseEvent::Button::kMiddle;
  WindowOpenDisposition disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  RunTest(browser(), GetHrefURL(), modifiers, button, disposition);
}

// Shift-middle-clicks open in a foreground tab.
IN_PROC_BROWSER_TEST_F(ClickModifierTest, HrefShiftMiddleClickTest) {
  int modifiers = blink::WebInputEvent::kShiftKey;
  blink::WebMouseEvent::Button button = blink::WebMouseEvent::Button::kMiddle;
  WindowOpenDisposition disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  RunTest(browser(), GetHrefURL(), modifiers, button, disposition);
}

IN_PROC_BROWSER_TEST_F(BrowserTest, GetSizeForNewRenderView) {
  // Force an initial resize. This works around a test-only problem on Chrome OS
  // where the shelf may not be created before the initial test browser window
  // opens, which leads to sizing issues in WebContents resize.
  browser()->window()->SetBounds(gfx::Rect(10, 20, 600, 400));
  // Let the message loop run so that resize actually takes effect.
  content::RunAllPendingInMessageLoop();

  // The instant extended NTP has javascript that does not work with
  // ui_test_utils::NavigateToURL.  The NTP rvh reloads when the browser tries
  // to navigate away from the page, which causes the WebContents to end up in
  // an inconsistent state. (is_loaded = true, last_commited_url=ntp,
  // visible_url=title1.html)
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kWebKitJavascriptEnabled,
                                               false);
  ASSERT_TRUE(embedded_test_server()->Start());
  // Create an HTTPS server for cross-site transition.
  net::EmbeddedTestServer https_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(https_test_server.Start());

  // Start with NTP.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab")));
  ASSERT_EQ(BookmarkBar::HIDDEN, browser()->bookmark_bar_state());
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderViewHost* prev_rvh =
      web_contents->GetPrimaryMainFrame()->GetRenderViewHost();
  const gfx::Size initial_wcv_size = web_contents->GetContainerBounds().size();
  RenderViewSizeObserver observer(web_contents, browser()->window());

  // Navigate to a non-NTP page, without resizing WebContentsView.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  ASSERT_EQ(BookmarkBar::HIDDEN, browser()->bookmark_bar_state());
  // A new RenderViewHost should be created.
  EXPECT_NE(prev_rvh, web_contents->GetPrimaryMainFrame()->GetRenderViewHost());
  prev_rvh = web_contents->GetPrimaryMainFrame()->GetRenderViewHost();
  gfx::Size rwhv_create_size0, rwhv_commit_size0, wcv_commit_size0;
  observer.GetSizeForRenderViewHost(
      web_contents->GetPrimaryMainFrame()->GetRenderViewHost(),
      &rwhv_create_size0, &rwhv_commit_size0, &wcv_commit_size0);
  EXPECT_EQ(gfx::Size(initial_wcv_size.width(), initial_wcv_size.height()),
            rwhv_create_size0);
  // When a navigation entry is committed, the size of RenderWidgetHostView
  // should be the same as when it was first created.
  EXPECT_EQ(rwhv_create_size0, rwhv_commit_size0);
  // Sizes of the current RenderWidgetHostView and WebContentsView should not
  // change before and after WebContentsObserver::DidFinishNavigation
  // (implemented by Browser); we obtain the sizes before PostCommit via
  // WebContentsObserver::NavigationEntryCommitted (implemented by
  // RenderViewSizeObserver).
  EXPECT_EQ(rwhv_commit_size0,
            web_contents->GetRenderWidgetHostView()->GetViewBounds().size());
// The behavior differs between OSX and views.
// In OSX, the wcv does not change size until after the commit, when the
// bookmark bar disappears (correct).
// In views, the wcv changes size at commit time.
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ(gfx::Size(wcv_commit_size0.width(), wcv_commit_size0.height()),
            web_contents->GetContainerBounds().size());
#else
  EXPECT_EQ(wcv_commit_size0, web_contents->GetContainerBounds().size());
#endif

  // Navigate to another non-NTP page, without resizing WebContentsView.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server.GetURL("/title2.html")));
  ASSERT_EQ(BookmarkBar::HIDDEN, browser()->bookmark_bar_state());
  // A new RenderVieHost should be created.
  EXPECT_NE(prev_rvh, web_contents->GetPrimaryMainFrame()->GetRenderViewHost());
  gfx::Size rwhv_create_size1, rwhv_commit_size1, wcv_commit_size1;
  observer.GetSizeForRenderViewHost(
      web_contents->GetPrimaryMainFrame()->GetRenderViewHost(),
      &rwhv_create_size1, &rwhv_commit_size1, &wcv_commit_size1);
  EXPECT_EQ(rwhv_create_size1, rwhv_commit_size1);
  EXPECT_EQ(rwhv_commit_size1,
            web_contents->GetRenderWidgetHostView()->GetViewBounds().size());
  EXPECT_EQ(wcv_commit_size1, web_contents->GetContainerBounds().size());

  // Navigate from NTP to a non-NTP page, resizing WebContentsView while
  // navigation entry is pending.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab")));
  gfx::Size wcv_resize_insets(1, 1);
  observer.set_wcv_resize_insets(wcv_resize_insets);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title2.html")));
  ASSERT_EQ(BookmarkBar::HIDDEN, browser()->bookmark_bar_state());
  gfx::Size rwhv_create_size2, rwhv_commit_size2, wcv_commit_size2;
  observer.GetSizeForRenderViewHost(
      web_contents->GetPrimaryMainFrame()->GetRenderViewHost(),
      &rwhv_create_size2, &rwhv_commit_size2, &wcv_commit_size2);

  // The behavior on OSX and Views is incorrect in this edge case, but they are
  // differently incorrect.
  // The behavior should be:
  // initial wcv size: (100,100)  (to choose random numbers)
  // initial rwhv size: (100,140)
  // commit wcv size: (101, 101)
  // commit rwhv size: (101, 141)
  // final wcv size: (101, 141)
  // final rwhv size: (101, 141)
  //
  // On OSX, the commit rwhv size is (101, 101)
  // On views, the commit wcv size is (101, 141)
  // All other sizes are correct.

  EXPECT_EQ(gfx::Size(initial_wcv_size.width(), initial_wcv_size.height()),
            rwhv_create_size2);
  gfx::Size exp_commit_size(initial_wcv_size);

#if BUILDFLAG(IS_MAC)
  exp_commit_size.Enlarge(wcv_resize_insets.width(),
                          wcv_resize_insets.height());
#else
  exp_commit_size.Enlarge(wcv_resize_insets.width(),
                          wcv_resize_insets.height());
#endif
  EXPECT_EQ(exp_commit_size, rwhv_commit_size2);
  EXPECT_EQ(exp_commit_size, wcv_commit_size2);

  gfx::Size exp_final_size(initial_wcv_size);
  exp_final_size.Enlarge(wcv_resize_insets.width(), wcv_resize_insets.height());
  EXPECT_EQ(exp_final_size,
            web_contents->GetRenderWidgetHostView()->GetViewBounds().size());
  EXPECT_EQ(exp_final_size, web_contents->GetContainerBounds().size());
}

IN_PROC_BROWSER_TEST_F(BrowserTest, CanDuplicateTab) {
  GURL url(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kTitle1File)));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED));

  int active_index = browser()->tab_strip_model()->active_index();
  EXPECT_EQ(0, active_index);

  EXPECT_TRUE(chrome::CanDuplicateTab(browser()));
  EXPECT_TRUE(chrome::CanDuplicateTabAt(browser(), 0));
  EXPECT_TRUE(chrome::CanDuplicateTabAt(browser(), 1));
}

namespace {

void CheckDisplayModeMQ(const std::u16string& display_mode,
                        content::WebContents* web_contents) {
  std::u16string function =
      u"(function() {return window.matchMedia('(display-mode: " + display_mode +
      u")').matches;})();";
  bool js_result = false;
  base::RunLoop run_loop;
  web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      function, base::BindLambdaForTesting([&](base::Value value) {
        DCHECK(value.is_bool());
        js_result = value.GetBool();
        run_loop.Quit();
      }),
      content::ISOLATED_WORLD_ID_GLOBAL);
  run_loop.Run();
  EXPECT_TRUE(js_result);
}

}  // namespace

// flaky new test: http://crbug.com/471703
IN_PROC_BROWSER_TEST_F(BrowserTest, DISABLED_ChangeDisplayMode) {
  CheckDisplayModeMQ(u"browser",
                     browser()->tab_strip_model()->GetActiveWebContents());

  Profile* profile = browser()->profile();
  Browser* app_browser = CreateBrowserForApp("blah", profile);
  auto* app_contents = app_browser->tab_strip_model()->GetActiveWebContents();
  CheckDisplayModeMQ(u"standalone", app_contents);

  app_browser->exclusive_access_manager()->context()->EnterFullscreen(
      GURL(), EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION,
      display::kInvalidDisplayId);

  // Sync navigation just to make sure IPC has passed (updated
  // display mode is delivered to RP).
  content::TestNavigationObserver observer(app_contents, 1);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(app_browser, GURL(url::kAboutBlankURL)));
  observer.Wait();

  CheckDisplayModeMQ(u"fullscreen", app_contents);
}

// Test to ensure the bounds of popup, devtool, and app windows are properly
// restored.
IN_PROC_BROWSER_TEST_F(BrowserTest, TestPopupBounds) {
  // TODO(tdanderson|pkasting): Change this to verify that the contents bounds
  // set by params.initial_bounds are the same as the contents bounds in the
  // initialized window. See crbug.com/585856.
  {
    // Minimum height a popup window should have added to the supplied content
    // bounds when drawn. This accommodates the browser toolbar.
    const int minimum_popup_padding = 26;

    // Creates an untrusted popup window and asserts that the eventual height is
    // padded with the toolbar and title bar height (initial height is content
    // height).
    Browser::CreateParams params(Browser::TYPE_POPUP, browser()->profile(),
                                 true);
    params.initial_bounds = gfx::Rect(0, 0, 100, 122);
    Browser* browser = Browser::Create(params);
    gfx::Rect bounds = browser->window()->GetBounds();

    // Should be EXPECT_EQ, but this width is inconsistent across platforms.
    // See https://crbug.com/567925.
    EXPECT_GE(bounds.width(), 100);

    // EXPECT_GE as Mac will have a larger height with the additional title bar.
    EXPECT_GE(bounds.height(), 122 + minimum_popup_padding);
    browser->window()->Close();
  }

  {
    // Creates a trusted popup window and asserts that the eventual height
    // doesn't change (initial height is window height).
    Browser::CreateParams params(Browser::TYPE_POPUP, browser()->profile(),
                                 true);
    params.initial_bounds = gfx::Rect(0, 0, 100, 122);
    params.trusted_source = true;
    Browser* browser = Browser::Create(params);
    gfx::Rect bounds = browser->window()->GetBounds();

    // Should be EXPECT_EQ, but this width is inconsistent across platforms.
    // See https://crbug.com/567925.
    EXPECT_GE(bounds.width(), 100);
    EXPECT_EQ(122, bounds.height());
    browser->window()->Close();
  }

  {
    // Creates an untrusted app window and asserts that the eventual height
    // doesn't change.
    Browser::CreateParams params = Browser::CreateParams::CreateForApp(
        "app-name", false, gfx::Rect(0, 0, 100, 122), browser()->profile(),
        true);
    Browser* browser = Browser::Create(params);
    gfx::Rect bounds = browser->window()->GetBounds();

    // Should be EXPECT_EQ, but this width is inconsistent across platforms.
    // See https://crbug.com/567925.
    EXPECT_GE(bounds.width(), 100);
    EXPECT_EQ(122, bounds.height());
    browser->window()->Close();
  }

  {
    // Creates a trusted app window and asserts that the eventual height
    // doesn't change.
    Browser::CreateParams params = Browser::CreateParams::CreateForApp(
        "app-name", true, gfx::Rect(0, 0, 100, 122), browser()->profile(),
        true);
    Browser* browser = Browser::Create(params);
    gfx::Rect bounds = browser->window()->GetBounds();

    // Should be EXPECT_EQ, but this width is inconsistent across platforms.
    // See https://crbug.com/567925.
    EXPECT_GE(bounds.width(), 100);
    EXPECT_EQ(122, bounds.height());
    browser->window()->Close();
  }

  {
    // Creates a devtools window and asserts that the eventual height
    // doesn't change.
    Browser::CreateParams params =
        Browser::CreateParams::CreateForDevTools(browser()->profile());
    params.initial_bounds = gfx::Rect(0, 0, 100, 122);
    Browser* browser = Browser::Create(params);
    gfx::Rect bounds = browser->window()->GetBounds();

    // Should be EXPECT_EQ, but this width is inconsistent across platforms.
    // See https://crbug.com/567925.
    EXPECT_GE(bounds.width(), 100);
    EXPECT_EQ(122, bounds.height());
    browser->window()->Close();
  }
}

IN_PROC_BROWSER_TEST_F(BrowserTest, IsOffTheRecordBrowserInUse) {
  EXPECT_FALSE(BrowserList::IsOffTheRecordBrowserInUse(browser()->profile()));

  Browser* incognito_browser = CreateIncognitoBrowser(browser()->profile());
  EXPECT_TRUE(BrowserList::IsOffTheRecordBrowserInUse(browser()->profile()));

  CloseBrowserSynchronously(incognito_browser);
  EXPECT_FALSE(BrowserList::IsOffTheRecordBrowserInUse(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(BrowserTest, TestActiveTabChangedUserAction) {
  base::UserActionTester user_action_tester;
  chrome::NewTab(browser());
  EXPECT_EQ(user_action_tester.GetActionCount("ActiveTabChanged"), 1);
}

IN_PROC_BROWSER_TEST_F(BrowserTest, TestNavEntryCommittedUserAction) {
  base::UserActionTester user_action_tester;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab")));
  EXPECT_EQ(user_action_tester.GetActionCount("NavEntryCommitted"), 1);
}

namespace {

void SetTestDefaultSearchProvider(TemplateURLService* service,
                                  const GURL& search_template) {
  ASSERT_TRUE(service);
  search_test_utils::WaitForTemplateURLServiceToLoad(service);
  ASSERT_TRUE(service->loaded());

  TemplateURLData data;
  data.SetShortName(u"test");
  data.SetKeyword(data.short_name());
  data.SetURL(search_template.spec());

  TemplateURL* template_url = service->Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(template_url);
  service->SetUserSelectedDefaultSearchProvider(template_url);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(BrowserTest,
                       TestNavEntryCommittedUserActionOnlyRecordedForTabs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  SetTestDefaultSearchProvider(
      service,
      embedded_test_server()->GetURL("a.test", "/title1.html?q={searchTerms}"));

  const GURL srp_url =
      service->GenerateSearchURLForDefaultSearchProvider(u"testing");

  base::UserActionTester user_action_tester;

  // Create and navigate a WebContents that is not a tab. The NavEntryCommitted
  // actions should not be recorded for this WebContents.
  std::unique_ptr<content::WebContents> non_tab_web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));
  content::TestNavigationObserver observer(non_tab_web_contents.get());
  non_tab_web_contents->GetController().LoadURL(
      srp_url, content::Referrer(), ::ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
      std::string());
  observer.Wait();

  EXPECT_EQ(user_action_tester.GetActionCount("NavEntryCommitted"), 0);
}

IN_PROC_BROWSER_TEST_F(BrowserTest, TestTabCountMetrics) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // This test assumes there's only one browser with one tab at the start of the
  // test.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  // Create an additional browser with two tabs.
  Browser* browser2 = CreateBrowser(browser()->profile());
  chrome::NewTab(browser2);
  ASSERT_TRUE(content::WaitForLoadStop(
      browser2->tab_strip_model()->GetActiveWebContents()));
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_EQ(2, browser2->tab_strip_model()->count());

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser2, embedded_test_server()->GetURL("/title1.html")));

  histogram_tester.ExpectBucketCount("Tabs.TabCountPerWindow", 1, 1);
  histogram_tester.ExpectBucketCount("Tabs.TabCountPerWindow", 2, 1);
  histogram_tester.ExpectUniqueSample("Tabs.TabCountPerLoad", 3, 1);
  // Skipping "Tabs.TabCountActiveWindow" due to flakiness risk w.r.t. focus.

  // Also check that tab group count metrics are recorded. In this case, there
  // aren't any groups.
  histogram_tester.ExpectUniqueSample("TabGroups.UserGroupCountPerLoad", 0, 1);
  histogram_tester.ExpectUniqueSample("TabGroups.UserPinnedTabCountPerLoad", 0,
                                      1);
  histogram_tester.ExpectUniqueSample("Tabs.TabCountInGroupPerLoad", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "TabGroups.UserCustomizedGroupCountPerLoad", 0, 1);
  histogram_tester.ExpectUniqueSample("TabGroups.CollapsedGroupCountPerLoad", 0,
                                      1);
}

IN_PROC_BROWSER_TEST_F(BrowserTest,
                       TestTabCountMetricsOnlyRecordedWhenTabLoads) {
  ASSERT_TRUE(embedded_test_server()->Start());

  base::HistogramTester histogram_tester;

  // Create and navigate a WebContents that is not a tab. Tab count metrics
  // should not be recorded when this WebContents loads.
  std::unique_ptr<content::WebContents> non_tab_web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));
  content::TestNavigationObserver observer(non_tab_web_contents.get());
  non_tab_web_contents->GetController().LoadURL(
      embedded_test_server()->GetURL("/title1.html"), content::Referrer(),
      ::ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
  observer.Wait();

  histogram_tester.ExpectTotalCount("Tabs.TabCountPerLoad", 0);
}

IN_PROC_BROWSER_TEST_F(BrowserTest,
                       TestTabCountMetricsNotRecordedWhenIframeLoads) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/iframe.html")));

  base::HistogramTester histogram_tester;

  // Tab count metrics should not be recorded when the iframe navigates.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  const GURL iframe_url = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(content::NavigateIframeToURL(web_contents, "test", iframe_url));

  histogram_tester.ExpectTotalCount("Tabs.TabCountPerLoad", 0);
}

IN_PROC_BROWSER_TEST_F(
    BrowserTest,
    TestTabCountMetricsNotRecordedForSameDocumentNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  base::HistogramTester histogram_tester;

  // Tab count metrics should not be recorded for same document navigations.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver nav_observer(web_contents);
  ASSERT_TRUE(content::ExecJs(web_contents, "location.href = '#test'"));
  nav_observer.Wait();

  histogram_tester.ExpectTotalCount("Tabs.TabCountPerLoad", 0);
}

IN_PROC_BROWSER_TEST_F(BrowserTest, TestTabCountMetricsRecordedOnReload) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  base::HistogramTester histogram_tester;

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver nav_observer(web_contents);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  nav_observer.Wait();

  histogram_tester.ExpectTotalCount("Tabs.TabCountPerLoad", 1);
}

IN_PROC_BROWSER_TEST_F(BrowserTest,
                       TestTabCountMetricsRecordedOnBackNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title2.html")));

  base::HistogramTester histogram_tester;

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver nav_observer(web_contents);
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  nav_observer.Wait();

  histogram_tester.ExpectTotalCount("Tabs.TabCountPerLoad", 1);
}

IN_PROC_BROWSER_TEST_F(BrowserTest, TestActiveBrowserChangedUserAction) {
  base::UserActionTester user_action_tester;
  BrowserList::SetLastActive(browser());
  EXPECT_EQ(user_action_tester.GetActionCount("ActiveBrowserChanged"), 1);
}

// DISABLED for flakiness. See https://crbug.com/1184168
IN_PROC_BROWSER_TEST_F(
    BrowserTest,
    DISABLED_SameDocumentNavigationWithNothingCommittedAfterCrash) {
  // The test sets this closure before each navigation to /sometimes-slow in
  // order to control the response for that navigation.
  content::SlowHttpResponse::GotRequestCallback got_slow_request;

  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url != "/sometimes-slow")
          return nullptr;
        DCHECK(got_slow_request)
            << "Set `got_slow_request` before each navigation request.";
        return std::make_unique<content::SlowHttpResponse>(
            std::move(got_slow_request));
      }));
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url1 = embedded_test_server()->GetURL("/sometimes-slow");
  GURL url2 = embedded_test_server()->GetURL("/sometimes-slow#foo");

  WebContents* wc = browser()->tab_strip_model()->GetActiveWebContents();

  // Successfully navigate to `url1`.
  got_slow_request = content::SlowHttpResponse::FinishResponseImmediately();
  EXPECT_TRUE(NavigateToURL(wc, url1));

  // Kill the renderer for the tab.
  {
    content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;

    content::RenderFrameDeletedObserver crash_observer(
        wc->GetPrimaryMainFrame());
    wc->GetPrimaryMainFrame()->GetProcess()->Shutdown(1);
    crash_observer.WaitUntilDeleted();
  }

  // Bring the process back to life for the current RenderFrameHost, though with
  // a speculative RenderFrameHost navigating back to `url1`.
  {
    content::NavigationController::LoadURLParams params(url1);
    params.transition_type = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);

    base::RunLoop loop;
    got_slow_request =
        base::BindLambdaForTesting([&](base::OnceClosure start_response,
                                       base::OnceClosure finish_response) {
          // Never starts the response, but informs the test the request has
          // been received.
          loop.Quit();
        });
    wc->GetController().LoadURLWithParams(params);
    loop.Run();
  }
  // The navigation has not completed, but the renderer has come alive.
  EXPECT_TRUE(wc->GetPrimaryMainFrame()->IsRenderFrameLive());
  EXPECT_EQ(wc->GetPrimaryMainFrame()->GetLastCommittedURL().spec(), "");

  // Now try to navigate to `url2`. We're currently trying to load `url1` since
  // the above navigation will be delayed. Going to `url2` should be a
  // same-document navigation according to the urls alone. But it can't be since
  // the current frame host does not actually have a document loaded.
  content::NavigationHandleCommitObserver nav_observer(wc, url2);
  {
    content::NavigationController::LoadURLParams params(url2);
    params.transition_type = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);

    got_slow_request = content::SlowHttpResponse::FinishResponseImmediately();
    wc->GetController().LoadURLWithParams(params);
  }
  EXPECT_TRUE(WaitForLoadStop(wc));
  EXPECT_TRUE(nav_observer.has_committed());
  EXPECT_FALSE(nav_observer.was_same_document());
}

// DISABLED for flakiness. See https://crbug.com/1184168
IN_PROC_BROWSER_TEST_F(
    BrowserTest,
    DISABLED_SameDocumentHistoryNavigationWithNothingCommittedAfterCrash) {
  content::SlowHttpResponse::GotRequestCallback got_slow_request;

  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url != "/sometimes-slow")
          return nullptr;
        DCHECK(got_slow_request)
            << "Set `got_slow_request` before each navigation request.";
        return std::make_unique<content::SlowHttpResponse>(
            std::move(got_slow_request));
      }));
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url1 = embedded_test_server()->GetURL("/sometimes-slow");
  GURL url2 = embedded_test_server()->GetURL("/sometimes-slow#foo");

  content::WebContents* wc =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Successfully navigate to `url1`, then do a same-document navigation to
  // `url2`.
  got_slow_request = content::SlowHttpResponse::FinishResponseImmediately();
  EXPECT_TRUE(NavigateToURL(wc, url1));
  EXPECT_TRUE(NavigateToURL(wc, url2));

  // Kill the renderer for the tab.
  {
    content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;

    content::RenderFrameDeletedObserver crash_observer(
        wc->GetPrimaryMainFrame());
    wc->GetPrimaryMainFrame()->GetProcess()->Shutdown(1);
    crash_observer.WaitUntilDeleted();
  }

  // Bring the process back to life for the current RenderFrameHost, though with
  // a speculative RenderFrameHost navigating back to `url1`.
  {
    content::NavigationController::LoadURLParams params(url1);
    params.transition_type = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);

    base::RunLoop loop;
    got_slow_request =
        base::BindLambdaForTesting([&](base::OnceClosure start_response,
                                       base::OnceClosure finish_response) {
          // Never starts the response, but informs the test the request has
          // been received.
          loop.Quit();
        });
    wc->GetController().LoadURLWithParams(params);
    loop.Run();
  }
  // The navigation has not completed, but the renderer has come alive.
  EXPECT_TRUE(wc->GetPrimaryMainFrame()->IsRenderFrameLive());
  EXPECT_EQ(wc->GetPrimaryMainFrame()->GetLastCommittedURL().spec(), "");

  content::NavigationHandleCommitObserver back_observer(wc, url1);
  // Now try to go back. We're currently at `url2` since the above navigation
  // will be blocked. Going back to `url1` should be a same-document history
  // navigation according to the NavigationEntry. But it can't be since the
  // current frame host does not actually have a document loaded.
  got_slow_request = content::SlowHttpResponse::FinishResponseImmediately();
  wc->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(wc));
  EXPECT_TRUE(back_observer.has_committed());
  EXPECT_FALSE(back_observer.was_same_document());
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(BrowserTest, CreatePictureInPicture) {
  Browser* popup_browser = Browser::Create(Browser::CreateParams(
      Browser::TYPE_PICTURE_IN_PICTURE, browser()->profile(), true));
  ASSERT_TRUE(popup_browser->is_type_picture_in_picture());
}
#endif  // !IS_CHROMEOS_LACROS

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(BrowserTest, PreventCloseYieldsCancelledEvent) {
  base::ScopedObservation<BrowserList, BrowserListObserver> observer(this);
  observer.Observe(BrowserList::GetInstance());

  base::test::TestFuture<void> policy_refresh_sync_future;
  web_app::WebAppProvider::GetForWebApps(profile())
      ->policy_manager()
      .SetRefreshPolicySettingsCompletedCallbackForTesting(
          policy_refresh_sync_future.GetCallback());

  const absl::Cleanup policy_cleanup = [this]() {
    // Clear policy values, otherwise we won't be able to gracefully close the
    // browser test.
    profile()->GetPrefs()->SetList(prefs::kWebAppSettings, base::Value::List());
  };

  // Set up policy values.
  static constexpr char kCalculatorAppUrl[] = "https://calculator.apps.chrome/";
  profile()->GetPrefs()->SetList(
      prefs::kWebAppSettings,
      base::Value::List().Append(
          base::Value::Dict()
              .Set(web_app::kManifestId, kCalculatorAppUrl)
              .Set(web_app::kRunOnOsLogin, web_app::kRunWindowed)
              .Set(web_app::kPreventClose, true)));
  profile()->GetPrefs()->SetList(
      prefs::kWebAppInstallForceList,
      base::Value::List().Append(
          base::Value::Dict()
              .Set(web_app::kUrlKey, kCalculatorAppUrl)
              .Set(web_app::kDefaultLaunchContainerKey,
                   web_app::kDefaultLaunchContainerWindowValue)));
  ASSERT_TRUE(policy_refresh_sync_future.Wait());

  apps::AppUpdateWaiter waiter(
      profile(), web_app::kCalculatorAppId,
      base::BindRepeating([](const apps::AppUpdate& update) {
        return update.AllowClose().has_value() && !update.AllowClose().value();
      }));
  waiter.Await();

  Browser* const browser =
      web_app::LaunchWebAppBrowser(profile(), web_app::kCalculatorAppId);
  ASSERT_TRUE(browser);

  EXPECT_EQ(BrowserClosingStatus::kDeniedByPolicy,
            browser->HandleBeforeClose());
  EXPECT_CALL(*this, OnBrowserCloseCancelled(
                         browser, BrowserClosingStatus::kDeniedByPolicy))
      .Times(1);
  browser->OnWindowClosing();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
