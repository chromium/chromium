// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search/search.h"
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
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_dialog_tab_helper.h"
#include "chrome/browser/ui/search/local_ntp_test_utils.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"
#include "chrome/browser/ui/tabs/pinned_tab_codec.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/app_modal/app_modal_dialog_queue.h"
#include "components/app_modal/javascript_app_modal_dialog.h"
#include "components/app_modal/native_app_modal_dialog.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/base_session_service_test_helper.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/common/language_detection_details.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_service.h"
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
#include "content/public/common/frame_navigate_params.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
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
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/ui_base_features.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#include "chrome/browser/ui/cocoa/test/run_loop_testing.h"
#include "ui/accelerated_widget_mac/ca_transaction_observer.h"
#endif

#if defined(OS_WIN)
#include "base/i18n/rtl.h"
#include "chrome/browser/browser_process.h"
#endif

using app_modal::AppModalDialogQueue;
using app_modal::JavaScriptAppModalDialog;
using base::ASCIIToUTF16;
using content::InterstitialPage;
using content::HostZoomMap;
using content::NavigationController;
using content::NavigationEntry;
using content::OpenURLParams;
using content::Referrer;
using content::WebContents;
using content::WebContentsObserver;
using extensions::Extension;

namespace {

const char* kBeforeUnloadHTML =
    "<html><head><title>beforeunload</title></head><body>"
    "<script>window.onbeforeunload=function(e){return 'foo'}</script>"
    "</body></html>";

const char* kOpenNewBeforeUnloadPage =
    "w=window.open(); w.onbeforeunload=function(e){return 'foo'};";

const base::FilePath::CharType* kTitle1File = FILE_PATH_LITERAL("title1.html");
const base::FilePath::CharType* kTitle2File = FILE_PATH_LITERAL("title2.html");

const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data");

// Given a page title, returns the expected window caption string.
base::string16 WindowCaptionFromPageTitle(const base::string16& page_title) {
#if defined(OS_MACOSX)
  // On Mac, we don't want to suffix the page title with the application name.
  if (page_title.empty())
    return l10n_util::GetStringUTF16(IDS_BROWSER_WINDOW_MAC_TAB_UNTITLED);
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
  TabClosingObserver() : closing_count_(0) {}

  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() != TabStripModelChange::kRemoved)
      return;

    for (const auto& delta : change.deltas()) {
      if (delta.remove.will_be_deleted)
        ++closing_count_;
    }
  }

  int closing_count() const { return closing_count_; }

 private:
  int closing_count_;

  DISALLOW_COPY_AND_ASSIGN(TabClosingObserver);
};

// Used by CloseWithAppMenuOpen. Invokes CloseWindow on the supplied browser.
void CloseWindowCallback(Browser* browser) {
  chrome::CloseWindow(browser);
}

// Used by CloseWithAppMenuOpen. Posts a CloseWindowCallback and shows the app
// menu.
void RunCloseWithAppMenuCallback(Browser* browser) {
  // ShowAppMenu is modal under views. Schedule a task that closes the window.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&CloseWindowCallback, browser));
  chrome::ShowAppMenu(browser);
}

// Displays "INTERSTITIAL" while the interstitial is attached.
// (InterstitialPage can be used in a test directly, but there would be no way
// to visually tell if it is showing or not.)
class TestInterstitialPage : public content::InterstitialPageDelegate {
 public:
  TestInterstitialPage(WebContents* tab, bool new_navigation, const GURL& url) {
    interstitial_page_ = InterstitialPage::Create(
        tab, new_navigation, url , this);
    interstitial_page_->Show();
  }
  ~TestInterstitialPage() override {}
  void Proceed() {
    interstitial_page_->Proceed();
  }
  void DontProceed() {
    interstitial_page_->DontProceed();
  }

  std::string GetHTMLContents() override { return "<h1>INTERSTITIAL</h1>"; }

 private:
  InterstitialPage* interstitial_page_;  // Owns us.
};

class RenderViewSizeObserver : public content::WebContentsObserver {
 public:
  RenderViewSizeObserver(content::WebContents* web_contents,
                         BrowserWindow* browser_window)
      : WebContentsObserver(web_contents),
        browser_window_(browser_window) {
  }

  void GetSizeForRenderViewHost(
      content::RenderViewHost* render_view_host,
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
  // is pending.
  void DidStartNavigation(
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
  // WebContentsDelegate::DidNavigateMainFramePostCommit is called.
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& details) override {
    content::RenderViewHost* rvh = web_contents()->GetRenderViewHost();
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
  BrowserWindow* browser_window_;  // Weak ptr.

  DISALLOW_COPY_AND_ASSIGN(RenderViewSizeObserver);
};

}  // namespace

class BrowserTest : public extensions::ExtensionBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  // In RTL locales wrap the page title with RTL embedding characters so that it
  // matches the value returned by GetWindowTitle().
  base::string16 LocaleWindowCaptionFromPageTitle(
      const base::string16& expected_title) {
    base::string16 page_title = WindowCaptionFromPageTitle(expected_title);
#if defined(OS_WIN)
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

  // Returns the app extension aptly named "App Test".
  const Extension* GetExtension() {
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(browser()->profile());
    for (const scoped_refptr<const extensions::Extension>& extension :
         registry->enabled_extensions()) {
      if (extension->name() == "App Test")
        return extension.get();
    }
    NOTREACHED();
    return NULL;
  }
};

// Launch the app on a page with no title, check that the app title was set
// correctly.
IN_PROC_BROWSER_TEST_F(BrowserTest, NoTitle) {
  ui_test_utils::NavigateToURL(
      browser(), ui_test_utils::GetTestUrl(
                     base::FilePath(base::FilePath::kCurrentDirectory),
                     base::FilePath(kTitle1File)));
  EXPECT_EQ(LocaleWindowCaptionFromPageTitle(ASCIIToUTF16("title1.html")),
            browser()->GetWindowTitleForCurrentTab(
                true /* include_app_name */));
  base::string16 tab_title;
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &tab_title));
  EXPECT_EQ(ASCIIToUTF16("title1.html"), tab_title);
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
  base::string16 tab_title;
  base::string16 test_title;
  for (const auto& c : cases) {
    SCOPED_TRACE(c.message);
    GURL url(prefix_url.spec() + c.suffix);
    test_title = ASCIIToUTF16("title1.html" + c.suffix);
    content::TitleWatcher title_watcher(
        browser()->tab_strip_model()->GetActiveWebContents(), test_title);
    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_EQ(test_title, title_watcher.WaitAndGetTitle());
  }
}

// Launch the app, navigate to a page with a title, check that the app title
// was set correctly.
IN_PROC_BROWSER_TEST_F(BrowserTest, Title) {
  ui_test_utils::NavigateToURL(
      browser(), ui_test_utils::GetTestUrl(
                     base::FilePath(base::FilePath::kCurrentDirectory),
                     base::FilePath(kTitle2File)));
  const base::string16 test_title(ASCIIToUTF16("Title Of Awesomeness"));
  EXPECT_EQ(LocaleWindowCaptionFromPageTitle(test_title),
            browser()->GetWindowTitleForCurrentTab(
                true /* include_app_name */));
  base::string16 tab_title;
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &tab_title));
  EXPECT_EQ(test_title, tab_title);
}

namespace {

class DialogPlusConsoleObserverDelegate
    : public content::ConsoleObserverDelegate {
 public:
  DialogPlusConsoleObserverDelegate(
      content::WebContentsDelegate* original_delegate,
      WebContents* web_contents,
      const std::string& filter)
      : content::ConsoleObserverDelegate(web_contents, filter),
        web_contents_(web_contents),
        original_delegate_(original_delegate) {}

  // WebContentsDelegate method:
  content::JavaScriptDialogManager* GetJavaScriptDialogManager(
      WebContents* source) override {
    return original_delegate_->GetJavaScriptDialogManager(web_contents_);
  }

 private:
  content::WebContents* web_contents_;
  content::WebContentsDelegate* original_delegate_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(BrowserTest, NoJavaScriptDialogsActivateTab) {
  // Set up two tabs, with the tab at index 0 active.
  GURL url(ui_test_utils::GetTestUrl(base::FilePath(
      base::FilePath::kCurrentDirectory), base::FilePath(kTitle1File)));
  ui_test_utils::NavigateToURL(browser(), url);
  AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  WebContents* second_tab = browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_TRUE(second_tab);
  content::WebContentsDelegate* original_delegate = second_tab->GetDelegate();

  // Show a confirm() dialog from the tab at index 1. The active index shouldn't
  // budge.
  DialogPlusConsoleObserverDelegate confirm_observer(
      original_delegate, second_tab, "*confirm*suppressed*");
  second_tab->SetDelegate(&confirm_observer);
  second_tab->GetMainFrame()->ExecuteJavaScriptForTests(
      ASCIIToUTF16("confirm('Activate!');"));
  confirm_observer.Wait();
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  // Show a prompt() dialog from the tab at index 1. The active index shouldn't
  // budge.
  DialogPlusConsoleObserverDelegate prompt_observer(
      original_delegate, second_tab, "*prompt*suppressed*");
  second_tab->SetDelegate(&prompt_observer);
  second_tab->GetMainFrame()->ExecuteJavaScriptForTests(
      ASCIIToUTF16("prompt('Activate!');"));
  prompt_observer.Wait();
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  second_tab->SetDelegate(original_delegate);

  // Show an alert() dialog from the tab at index 1. The active index shouldn't
  // budge.
  JavaScriptDialogTabHelper* js_helper =
      JavaScriptDialogTabHelper::FromWebContents(second_tab);
  base::RunLoop alert_wait;
  js_helper->SetDialogShownCallbackForTesting(alert_wait.QuitClosure());
  second_tab->GetMainFrame()->ExecuteJavaScriptForTests(
      ASCIIToUTF16("alert('Activate!');"));
  alert_wait.Run();
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
}

#if defined(OS_WIN) && !defined(NDEBUG)
// http://crbug.com/114859. Times out frequently on Windows.
#define MAYBE_ThirtyFourTabs DISABLED_ThirtyFourTabs
#else
#define MAYBE_ThirtyFourTabs ThirtyFourTabs
#endif

// Create 34 tabs and verify that a lot of processes have been created. The
// exact number of processes depends on the amount of memory. Previously we
// had a hard limit of 31 processes and this test is mainly directed at
// verifying that we don't crash when we pass this limit.
// Warning: this test can take >30 seconds when running on a slow (low
// memory?) Mac builder.
IN_PROC_BROWSER_TEST_F(BrowserTest, MAYBE_ThirtyFourTabs) {
  GURL url(ui_test_utils::GetTestUrl(base::FilePath(
      base::FilePath::kCurrentDirectory), base::FilePath(kTitle2File)));

  // There is one initial tab.
  const int kTabCount = 34;
  for (int ix = 0; ix != (kTabCount - 1); ++ix) {
    chrome::AddSelectedTabWithURL(browser(), url,
                                  ui::PAGE_TRANSITION_TYPED);
  }
  EXPECT_EQ(kTabCount, browser()->tab_strip_model()->count());

  // See GetMaxRendererProcessCount() in
  // content/browser/renderer_host/render_process_host_impl.cc
  // for the algorithm to decide how many processes to create.
  const int kExpectedProcessCount =
#if defined(ARCH_CPU_64_BITS)
      17;
#else
      25;
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
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  // Navigate to a 204 URL (aborts with no content) on the NTP and make sure it
  // sticks around so that the user can edit it.
  GURL abort_url(embedded_test_server()->GetURL("/nocontent"));
  {
    content::WindowedNotificationObserver stop_observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(
            &web_contents->GetController()));
    browser()->OpenURL(OpenURLParams(abort_url, Referrer(),
                                     WindowOpenDisposition::CURRENT_TAB,
                                     ui::PAGE_TRANSITION_TYPED, false));
    stop_observer.Wait();
    EXPECT_TRUE(web_contents->GetController().GetPendingEntry());
    EXPECT_EQ(abort_url, web_contents->GetVisibleURL());
  }

  // Navigate to a real URL.
  GURL real_url(embedded_test_server()->GetURL("/title1.html"));
  ui_test_utils::NavigateToURL(browser(), real_url);
  EXPECT_EQ(real_url, web_contents->GetVisibleURL());

  // Now navigating to a 204 URL should clear the pending entry.
  {
    content::WindowedNotificationObserver stop_observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(
            &web_contents->GetController()));
    browser()->OpenURL(OpenURLParams(abort_url, Referrer(),
                                     WindowOpenDisposition::CURRENT_TAB,
                                     ui::PAGE_TRANSITION_TYPED, false));
    stop_observer.Wait();
    EXPECT_FALSE(web_contents->GetController().GetPendingEntry());
    EXPECT_EQ(real_url, web_contents->GetVisibleURL());
  }
}

// Test for crbug.com/297289.  Ensure that modal dialogs are closed when a
// cross-process navigation is ready to commit.
// Flaky test, see https://crbug.com/445155.
IN_PROC_BROWSER_TEST_F(BrowserTest, DISABLED_CrossProcessNavCancelsDialogs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/empty.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  // Test this with multiple alert dialogs to ensure that we can navigate away
  // even if the renderer tries to synchronously create more.
  // See http://crbug.com/312490.
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  JavaScriptDialogTabHelper* js_helper =
      JavaScriptDialogTabHelper::FromWebContents(contents);
  base::RunLoop dialog_wait;
  js_helper->SetDialogShownCallbackForTesting(dialog_wait.QuitClosure());
  contents->GetMainFrame()->ExecuteJavaScriptForTests(
      ASCIIToUTF16("alert('one'); alert('two');"));
  dialog_wait.Run();
  EXPECT_TRUE(js_helper->IsShowingDialogForTesting());

  // A cross-site navigation should force the dialog to close.
  GURL url2("http://www.example.com/empty.html");
  ui_test_utils::NavigateToURL(browser(), url2);
  EXPECT_FALSE(js_helper->IsShowingDialogForTesting());

  // Make sure input events still work in the renderer process.
  EXPECT_FALSE(contents->GetMainFrame()->GetProcess()->IgnoreInputEvents());
}

// Make sure that dialogs are closed after a renderer process dies, and that
// subsequent navigations work.  See http://crbug/com/343265.
IN_PROC_BROWSER_TEST_F(BrowserTest, SadTabCancelsDialogs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL beforeunload_url(embedded_test_server()->GetURL("/beforeunload.html"));
  ui_test_utils::NavigateToURL(browser(), beforeunload_url);
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::PrepContentsForBeforeUnloadTest(contents);

  // Start a navigation to trigger the beforeunload dialog.
  contents->GetMainFrame()->ExecuteJavaScriptForTests(
      ASCIIToUTF16("window.location.href = 'about:blank'"));
  JavaScriptAppModalDialog* alert = ui_test_utils::WaitForAppModalDialog();
  EXPECT_TRUE(alert->IsValid());
  AppModalDialogQueue* dialog_queue = AppModalDialogQueue::GetInstance();
  EXPECT_TRUE(dialog_queue->HasActiveDialog());

  // Crash the renderer process and ensure the dialog is gone.
  content::RenderProcessHost* child_process =
      contents->GetMainFrame()->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      child_process,
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0);
  crash_observer.Wait();
  EXPECT_FALSE(dialog_queue->HasActiveDialog());

  // Make sure subsequent navigations work.
  GURL url2("http://www.example.com/empty.html");
  ui_test_utils::NavigateToURL(browser(), url2);
}

// Make sure that dialogs opened by subframes are closed when the process dies.
// See http://crbug.com/366510.
IN_PROC_BROWSER_TEST_F(BrowserTest, SadTabCancelsSubframeDialogs) {
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html, <html><body></body></html>"));

  // Create an iframe that opens an alert dialog.
  JavaScriptDialogTabHelper* js_helper =
      JavaScriptDialogTabHelper::FromWebContents(contents);
  base::RunLoop dialog_wait;
  js_helper->SetDialogShownCallbackForTesting(dialog_wait.QuitClosure());
  contents->GetMainFrame()->ExecuteJavaScriptForTests(
      ASCIIToUTF16("f = document.createElement('iframe');"
                   "f.srcdoc = '<script>alert(1)</script>';"
                   "document.body.appendChild(f);"));
  dialog_wait.Run();
  EXPECT_TRUE(js_helper->IsShowingDialogForTesting());

  // Crash the renderer process and ensure the dialog is gone.
  content::RenderProcessHost* child_process =
      contents->GetMainFrame()->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      child_process,
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0);
  crash_observer.Wait();
  EXPECT_FALSE(js_helper->IsShowingDialogForTesting());

  // Make sure subsequent navigations work.
  GURL url2("data:text/html,foo");
  ui_test_utils::NavigateToURL(browser(), url2);
}

// Make sure modal dialogs within a guestview are closed when an interstitial
// page is showing. See crbug.com/482380.
IN_PROC_BROWSER_TEST_F(BrowserTest, InterstitialCancelsGuestViewDialogs) {
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  JavaScriptDialogTabHelper* js_helper =
      JavaScriptDialogTabHelper::FromWebContents(contents);
  base::RunLoop dialog_wait;
  js_helper->SetDialogShownCallbackForTesting(dialog_wait.QuitClosure());

  // Navigate to a PDF, which is loaded within a guestview.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL pdf_with_dialog(embedded_test_server()->GetURL("/alert_dialog.pdf"));
  ui_test_utils::NavigateToURL(browser(), pdf_with_dialog);

  dialog_wait.Run();
  EXPECT_TRUE(js_helper->IsShowingDialogForTesting());

  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents, false, GURL());
  content::WaitForInterstitialAttach(contents);

  // The interstitial should have closed the dialog.
  EXPECT_TRUE(contents->ShowingInterstitialPage());
  EXPECT_FALSE(js_helper->IsShowingDialogForTesting());

  interstitial->DontProceed();
}

// Test for crbug.com/22004.  Reloading a page with a before unload handler and
// then canceling the dialog should not leave the throbber spinning.
// https://crbug.com/898370: Test is flakily timing out
IN_PROC_BROWSER_TEST_F(BrowserTest, DISABLED_ReloadThenCancelBeforeUnload) {
  GURL url(std::string("data:text/html,") + kBeforeUnloadHTML);
  ui_test_utils::NavigateToURL(browser(), url);
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::PrepContentsForBeforeUnloadTest(contents);

  // Navigate to another page, but click cancel in the dialog.  Make sure that
  // the throbber stops spinning.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  JavaScriptAppModalDialog* alert = ui_test_utils::WaitForAppModalDialog();

  alert->CloseModalDialog();
  EXPECT_FALSE(contents->IsLoading());

  // Clear the beforeunload handler so the test can easily exit.
  contents->GetMainFrame()->ExecuteJavaScriptForTests(
      ASCIIToUTF16("onbeforeunload=null;"));
}

// Test for crbug.com/11647.  A page closed with window.close() should not have
// two beforeunload dialogs shown.
// http://crbug.com/410891
IN_PROC_BROWSER_TEST_F(BrowserTest,
                       DISABLED_SingleBeforeUnloadAfterWindowClose) {
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->GetMainFrame()
      ->ExecuteJavaScriptWithUserGestureForTests(
          ASCIIToUTF16(kOpenNewBeforeUnloadPage));

  // Close the new window with JavaScript, which should show a single
  // beforeunload dialog.  Then show another alert, to make it easy to verify
  // that a second beforeunload dialog isn't shown.
  browser()
      ->tab_strip_model()
      ->GetWebContentsAt(0)
      ->GetMainFrame()
      ->ExecuteJavaScriptWithUserGestureForTests(
          ASCIIToUTF16("w.close(); alert('bar');"));
  JavaScriptAppModalDialog* alert = ui_test_utils::WaitForAppModalDialog();
  alert->native_dialog()->AcceptAppModalDialog();

  alert = ui_test_utils::WaitForAppModalDialog();
  EXPECT_FALSE(alert->is_before_unload_dialog());
  alert->native_dialog()->AcceptAppModalDialog();
}

// Test that when a page has an onbeforeunload handler, reloading a page shows a
// different dialog than navigating to a different page.
IN_PROC_BROWSER_TEST_F(BrowserTest, BeforeUnloadVsBeforeReload) {
  GURL url(std::string("data:text/html,") + kBeforeUnloadHTML);
  ui_test_utils::NavigateToURL(browser(), url);
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::PrepContentsForBeforeUnloadTest(contents);

  // Reload the page, and check that we get a "before reload" dialog.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  JavaScriptAppModalDialog* alert = ui_test_utils::WaitForAppModalDialog();
  EXPECT_TRUE(alert->is_reload());

  // Proceed with the reload.
  alert->native_dialog()->AcceptAppModalDialog();
  EXPECT_TRUE(content::WaitForLoadStop(contents));

  content::PrepContentsForBeforeUnloadTest(contents);

  // Navigate to another url, and check that we get a "before unload" dialog.
  GURL url2(url::kAboutBlankURL);
  browser()->OpenURL(OpenURLParams(url2, Referrer(),
                                   WindowOpenDisposition::CURRENT_TAB,
                                   ui::PAGE_TRANSITION_TYPED, false));

  alert = ui_test_utils::WaitForAppModalDialog();
  EXPECT_FALSE(alert->is_reload());

  // Accept the navigation so we end up on a page without a beforeunload hook.
  alert->native_dialog()->AcceptAppModalDialog();
}

// BeforeUnloadAtQuitWithTwoWindows is a regression test for
// http://crbug.com/11842. It opens two windows, one of which has a
// beforeunload handler and attempts to exit cleanly.
class BeforeUnloadAtQuitWithTwoWindows : public InProcessBrowserTest {
 public:
  // This test is for testing a specific shutdown behavior. This mimics what
  // happens in InProcessBrowserTest::RunTestOnMainThread and QuitBrowsers, but
  // ensures that it happens through the single IDC_EXIT of the test.
  void TearDownOnMainThread() override {
    // Cycle both the MessageLoop and the Cocoa runloop twice to flush out any
    // Chrome work that generates Cocoa work. Do this twice since there are two
    // Browsers that must be closed.
    CycleRunLoops();
    CycleRunLoops();

    // Run the application event loop to completion, which will cycle the
    // native MessagePump on all platforms.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::RunLoop::QuitCurrentWhenIdleClosureDeprecated());
    base::RunLoop().Run();

    // Take care of any remaining Cocoa work.
    CycleRunLoops();

    // At this point, quit should be for real now.
    ASSERT_EQ(0u, chrome::GetTotalBrowserCount());
  }

  // A helper function that cycles the MessageLoop, and on Mac, the Cocoa run
  // loop. It also drains the NSAutoreleasePool.
  void CycleRunLoops() {
    content::RunAllPendingInMessageLoop();
#if defined(OS_MACOSX)
    chrome::testing::NSRunLoopRunAllPending();
    AutoreleasePool()->Recycle();
#endif
  }
};

// Disabled, http://crbug.com/159214 .
IN_PROC_BROWSER_TEST_F(BeforeUnloadAtQuitWithTwoWindows,
                       DISABLED_IfThisTestTimesOutItIndicatesFAILURE) {
  // In the first browser, set up a page that has a beforeunload handler.
  GURL url(std::string("data:text/html,") + kBeforeUnloadHTML);
  ui_test_utils::NavigateToURL(browser(), url);
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::PrepContentsForBeforeUnloadTest(contents);

  // Open a second browser window at about:blank.
  ui_test_utils::BrowserAddedObserver browser_added_observer;
  chrome::NewEmptyWindow(browser()->profile());
  Browser* second_window = browser_added_observer.WaitForSingleNewBrowser();
  ui_test_utils::NavigateToURL(second_window, GURL(url::kAboutBlankURL));

  // Tell the application to quit. IDC_EXIT calls AttemptUserExit, which on
  // everything but ChromeOS allows unload handlers to block exit. On that
  // platform, though, it exits unconditionally. See the comment and bug ID
  // in AttemptUserExit() in application_lifetime.cc.
#if defined(OS_CHROMEOS)
  chrome::AttemptExit();
#else
  chrome::ExecuteCommand(second_window, IDC_EXIT);
#endif

  // The beforeunload handler will run at exit, ensure it does, and then accept
  // it to allow shutdown to proceed.
  JavaScriptAppModalDialog* alert = ui_test_utils::WaitForAppModalDialog();
  ASSERT_TRUE(alert);
  EXPECT_TRUE(alert->is_before_unload_dialog());
  alert->native_dialog()->AcceptAppModalDialog();

  // But wait there's more! If this test times out, it likely means that the
  // browser has not been able to quit correctly, indicating there's a
  // regression of the bug noted above.
}

// Test that scripts can fork a new renderer process for a cross-site popup,
// based on http://www.google.com/chrome/intl/en/webmasters-faq.html#newtab.
// The script must open a new tab, set its window.opener to null, and navigate
// it to a cross-site URL.  It should also work for meta-refreshes.
// See http://crbug.com/93517.
IN_PROC_BROWSER_TEST_F(BrowserTest, NullOpenerRedirectForksProcess) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisablePopupBlocking);

  // Create http and https servers for a cross-site transition.
  ASSERT_TRUE(embedded_test_server()->Start());
  net::EmbeddedTestServer https_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));
  ASSERT_TRUE(https_test_server.Start());
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  GURL https_url(https_test_server.GetURL("/title2.html"));

  // Start with an http URL.
  ui_test_utils::NavigateToURL(browser(), http_url);
  WebContents* oldtab = browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderProcessHost* process = oldtab->GetMainFrame()->GetProcess();

  // Now open a tab to a blank page, set its opener to null, and redirect it
  // cross-site.
  std::string redirect_popup = "w=window.open();";
  redirect_popup += "w.opener=null;";
  redirect_popup += "w.document.location=\"";
  redirect_popup += https_url.spec();
  redirect_popup += "\";";

  content::WindowedNotificationObserver popup_observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());
  content::WindowedNotificationObserver nav_observer(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  oldtab->GetMainFrame()->
      ExecuteJavaScriptWithUserGestureForTests(ASCIIToUTF16(redirect_popup));

  // Wait for popup window to appear and finish navigating.
  popup_observer.Wait();
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  WebContents* newtab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(newtab);
  EXPECT_NE(oldtab, newtab);
  nav_observer.Wait();
  ASSERT_TRUE(newtab->GetController().GetLastCommittedEntry());
  EXPECT_EQ(https_url.spec(),
            newtab->GetController().GetLastCommittedEntry()->GetURL().spec());

  // Popup window should not be in the opener's process.
  content::RenderProcessHost* popup_process =
      newtab->GetMainFrame()->GetProcess();
  EXPECT_NE(process, popup_process);

  // Now open a tab to a blank page, set its opener to null, and use a
  // meta-refresh to navigate it instead.
  std::string refresh_popup = "w=window.open();";
  refresh_popup += "w.opener=null;";
  refresh_popup += "w.document.write(";
  refresh_popup += "'<META HTTP-EQUIV=\"refresh\" content=\"0; url=";
  refresh_popup += https_url.spec();
  refresh_popup += "\">');w.document.close();";

  content::WindowedNotificationObserver popup_observer2(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());
  content::WindowedNotificationObserver nav_observer2(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  oldtab->GetMainFrame()->
      ExecuteJavaScriptWithUserGestureForTests(ASCIIToUTF16(refresh_popup));

  // Wait for popup window to appear and finish navigating.
  popup_observer2.Wait();
  ASSERT_EQ(3, browser()->tab_strip_model()->count());
  WebContents* newtab2 = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(newtab2);
  EXPECT_NE(oldtab, newtab2);
  nav_observer2.Wait();
  ASSERT_TRUE(newtab2->GetController().GetLastCommittedEntry());
  EXPECT_EQ(https_url.spec(),
            newtab2->GetController().GetLastCommittedEntry()->GetURL().spec());

  // This popup window should also not be in the opener's process.
  content::RenderProcessHost* popup_process2 =
      newtab2->GetMainFrame()->GetProcess();
  EXPECT_NE(process, popup_process2);
}

// Tests that other popup navigations that do not follow the steps at
// http://www.google.com/chrome/intl/en/webmasters-faq.html#newtab will not
// fork a new renderer process.
IN_PROC_BROWSER_TEST_F(BrowserTest, OtherRedirectsDontForkProcess) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisablePopupBlocking);

  // Create http and https servers for a cross-site transition.
  ASSERT_TRUE(embedded_test_server()->Start());
  net::EmbeddedTestServer https_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));
  ASSERT_TRUE(https_test_server.Start());
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  GURL https_url(https_test_server.GetURL("/title2.html"));

  // Start with an http URL.
  ui_test_utils::NavigateToURL(browser(), http_url);
  WebContents* oldtab = browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderProcessHost* process = oldtab->GetMainFrame()->GetProcess();

  // Now open a tab to a blank page and redirect it cross-site.
  std::string dont_fork_popup = "w=window.open();";
  dont_fork_popup += "w.document.location=\"";
  dont_fork_popup += https_url.spec();
  dont_fork_popup += "\";";

  content::WindowedNotificationObserver popup_observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());
  content::WindowedNotificationObserver nav_observer(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  oldtab->GetMainFrame()->
      ExecuteJavaScriptWithUserGestureForTests(ASCIIToUTF16(dont_fork_popup));

  // Wait for popup window to appear and finish navigating.
  popup_observer.Wait();
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  WebContents* newtab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(newtab);
  EXPECT_NE(oldtab, newtab);
  nav_observer.Wait();
  ASSERT_TRUE(newtab->GetController().GetLastCommittedEntry());
  EXPECT_EQ(https_url.spec(),
            newtab->GetController().GetLastCommittedEntry()->GetURL().spec());

  // Process of the (cross-site) popup window depends on whether
  // site-per-process mode is enabled or not.
  content::RenderProcessHost* popup_process =
      newtab->GetMainFrame()->GetProcess();
  if (content::AreAllSitesIsolatedForTesting())
    EXPECT_NE(process, popup_process);
  else
    EXPECT_EQ(process, popup_process);

  // Same thing if the current tab tries to navigate itself.
  std::string navigate_str = "document.location=\"";
  navigate_str += https_url.spec();
  navigate_str += "\";";

  content::WindowedNotificationObserver nav_observer2(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::NotificationService::AllSources());
  oldtab->GetMainFrame()->ExecuteJavaScriptWithUserGestureForTests(
      ASCIIToUTF16(navigate_str));
  nav_observer2.Wait();
  ASSERT_TRUE(oldtab->GetController().GetLastCommittedEntry());
  EXPECT_EQ(https_url.spec(),
            oldtab->GetController().GetLastCommittedEntry()->GetURL().spec());

  // Whether original stays in the original process (when navigating to a
  // cross-site url) depends on whether site-per-process mode is enabled or not.
  content::RenderProcessHost* new_process =
      newtab->GetMainFrame()->GetProcess();
  if (content::AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(process, new_process);

    // site-per-process should reuse the process for the https site.
    EXPECT_EQ(popup_process, new_process);
  } else {
    EXPECT_EQ(process, new_process);
  }
}

// Test that get_process_idle_time() returns reasonable values when compared
// with time deltas measured locally.
IN_PROC_BROWSER_TEST_F(BrowserTest, RenderIdleTime) {
  base::TimeTicks start = base::TimeTicks::Now();
  ui_test_utils::NavigateToURL(
      browser(), ui_test_utils::GetTestUrl(
                     base::FilePath(base::FilePath::kCurrentDirectory),
                     base::FilePath(kTitle1File)));
  base::TimeDelta renderer_td = browser()
                                    ->tab_strip_model()
                                    ->GetActiveWebContents()
                                    ->GetMainFrame()
                                    ->GetProcess()
                                    ->GetChildProcessIdleTime();
  base::TimeDelta browser_td = base::TimeTicks::Now() - start;
  EXPECT_TRUE(browser_td >= renderer_td);
}

// Test RenderView correctly send back favicon url for web page that redirects
// to an anchor in javascript body.onload handler.
IN_PROC_BROWSER_TEST_F(BrowserTest,
                       DISABLED_FaviconOfOnloadRedirectToAnchorPage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/onload_redirect_to_anchor.html"));
  GURL expected_favicon_url(embedded_test_server()->GetURL("/test.png"));

  ui_test_utils::NavigateToURL(browser(), url);

  NavigationEntry* entry = browser()->tab_strip_model()->
      GetActiveWebContents()->GetController().GetLastCommittedEntry();
  EXPECT_EQ(expected_favicon_url.spec(), entry->GetFavicon().url.spec());
}

#if defined(OS_MACOSX) || defined(OS_LINUX) || defined (OS_WIN)
// http://crbug.com/83828. On Mac 10.6, the failure rate is 14%
#define MAYBE_FaviconChange DISABLED_FaviconChange
#else
#define MAYBE_FaviconChange FaviconChange
#endif
// Test that an icon can be changed from JS.
IN_PROC_BROWSER_TEST_F(BrowserTest, MAYBE_FaviconChange) {
  static const base::FilePath::CharType* kFile =
      FILE_PATH_LITERAL("onload_change_favicon.html");
  GURL file_url(ui_test_utils::GetTestUrl(base::FilePath(
      base::FilePath::kCurrentDirectory), base::FilePath(kFile)));
  ASSERT_TRUE(file_url.SchemeIs(url::kFileScheme));
  ui_test_utils::NavigateToURL(browser(), file_url);

  NavigationEntry* entry = browser()->tab_strip_model()->
      GetActiveWebContents()->GetController().GetLastCommittedEntry();
  static const base::FilePath::CharType* kIcon =
      FILE_PATH_LITERAL("test1.png");
  GURL expected_favicon_url(ui_test_utils::GetTestUrl(base::FilePath(
      base::FilePath::kCurrentDirectory), base::FilePath(kIcon)));
  EXPECT_EQ(expected_favicon_url.spec(), entry->GetFavicon().url.spec());
}

// http://crbug.com/172336
#if defined(OS_WIN)
#define MAYBE_TabClosingWhenRemovingExtension \
    DISABLED_TabClosingWhenRemovingExtension
#else
#define MAYBE_TabClosingWhenRemovingExtension TabClosingWhenRemovingExtension
#endif
// Makes sure TabClosing is sent when uninstalling an extension that is an app
// tab.
IN_PROC_BROWSER_TEST_F(BrowserTest, MAYBE_TabClosingWhenRemovingExtension) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/empty.html"));
  TabStripModel* model = browser()->tab_strip_model();

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app/")));

  const Extension* extension_app = GetExtension();

  ui_test_utils::NavigateToURL(browser(), url);

  std::unique_ptr<WebContents> app_contents =
      WebContents::Create(WebContents::CreateParams(browser()->profile()));
  extensions::TabHelper::CreateForWebContents(app_contents.get());
  extensions::TabHelper* extensions_tab_helper =
      extensions::TabHelper::FromWebContents(app_contents.get());
  extensions_tab_helper->SetExtensionApp(extension_app);

  model->AddWebContents(std::move(app_contents), 0,
                        ui::PageTransitionFromInt(0), TabStripModel::ADD_NONE);
  model->SetTabPinned(0, true);
  ui_test_utils::NavigateToURL(browser(), url);

  TabClosingObserver observer;
  model->AddObserver(&observer);

  // Uninstall the extension and make sure TabClosing is sent.
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(browser()->profile())
          ->extension_service();
  service->UninstallExtension(GetExtension()->id(),
                              extensions::UNINSTALL_REASON_FOR_TESTING,
                              NULL);
  EXPECT_EQ(1, observer.closing_count());

  model->RemoveObserver(&observer);

  // There should only be one tab now.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
}

// Open with --app-id=<id>, and see that an application tab opens by default.
IN_PROC_BROWSER_TEST_F(BrowserTest, AppIdSwitch) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // There should be one tab to start with.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  // Load an app.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app/")));
  const Extension* extension_app = GetExtension();

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());

  chrome::startup::IsFirstRun first_run = first_run::IsChromeFirstRun() ?
      chrome::startup::IS_FIRST_RUN : chrome::startup::IS_NOT_FIRST_RUN;
  StartupBrowserCreatorImpl launch(base::FilePath(), command_line, first_run);

  bool new_bookmark_apps_enabled = extensions::util::IsNewBookmarkAppsEnabled();

  // If the new bookmark app flow is enabled, the app should open as an tab.
  // Otherwise the app should open as an app window.
  EXPECT_EQ(!new_bookmark_apps_enabled,
            launch.OpenApplicationWindow(browser()->profile()));
  EXPECT_EQ(new_bookmark_apps_enabled,
            launch.OpenApplicationTab(browser()->profile()));

  // Check that a the number of browsers and tabs is correct.
  unsigned int expected_browsers = 1;
  int expected_tabs = 1;
  new_bookmark_apps_enabled ? expected_tabs++ : expected_browsers++;

  EXPECT_EQ(expected_browsers, chrome::GetBrowserCount(browser()->profile()));
  EXPECT_EQ(expected_tabs, browser()->tab_strip_model()->count());
}

// Open an app window and the dev tools window and ensure that the location
// bar settings are correct.
IN_PROC_BROWSER_TEST_F(BrowserTest, ShouldShowLocationBar) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load an app.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app/")));
  const Extension* extension_app = GetExtension();

  // Launch it in a window, as AppLauncherHandler::HandleLaunchApp() would.
  WebContents* app_window = OpenApplication(AppLaunchParams(
      browser()->profile(), extension_app, extensions::LAUNCH_CONTAINER_WINDOW,
      WindowOpenDisposition::NEW_WINDOW, extensions::SOURCE_TEST));
  ASSERT_TRUE(app_window);

  DevToolsWindow* devtools_window =
      DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), false);

  // The launch should have created a new app browser and a dev tools browser.
  ASSERT_EQ(3u, chrome::GetBrowserCount(browser()->profile()));

  // Find the new browsers.
  Browser* app_browser = NULL;
  Browser* dev_tools_browser = NULL;
  for (auto* b : *BrowserList::GetInstance()) {
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
  EXPECT_TRUE(
      app_browser->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR));

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools_window);
}

// Regression test for crbug.com/702505.
// Fails occasionally on Mac. http://crbug.com/852697
#if defined(OS_MACOSX)
#define MAYBE_ReattachDevToolsWindow DISABLED_ReattachDevToolsWindow
#else
#define MAYBE_ReattachDevToolsWindow ReattachDevToolsWindow
#endif
IN_PROC_BROWSER_TEST_F(BrowserTest, MAYBE_ReattachDevToolsWindow) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

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
  content::RunAllPendingInMessageLoop();
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
#if !defined(OS_CHROMEOS)
// Makes sure pinned tabs are restored correctly on start.
IN_PROC_BROWSER_TEST_F(BrowserTest, RestorePinnedTabs) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Add a pinned tab.
  GURL url(embedded_test_server()->GetURL("/empty.html"));
  TabStripModel* model = browser()->tab_strip_model();
  ui_test_utils::NavigateToURL(browser(), url);
  model->SetTabPinned(0, true);

  // Add a non pinned tab.
  chrome::NewTab(browser());
  ui_test_utils::NavigateToURL(browser(), url);

  // Add another pinned tab.
  chrome::NewTab(browser());
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  model->SetTabPinned(2, true);

  // Write out the pinned tabs.
  PinnedTabCodec::WritePinnedTabs(browser()->profile());

  // Close the browser window.
  browser()->window()->Close();

  // Launch again with the same profile.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  chrome::startup::IsFirstRun first_run = first_run::IsChromeFirstRun() ?
      chrome::startup::IS_FIRST_RUN : chrome::startup::IS_NOT_FIRST_RUN;
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, first_run);
  launch.Launch(browser()->profile(), std::vector<GURL>(), false);

  // The launch should have created a new browser.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));

  // Find the new browser.
  BrowserList* browsers = BrowserList::GetInstance();
  auto new_browser_iter =
      std::find_if(browsers->begin(), browsers->end(),
                   [this](Browser* b) { return b != browser(); });
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
#endif  // !defined(OS_CHROMEOS)

// This test verifies we don't crash when closing the last window and the app
// menu is showing.
IN_PROC_BROWSER_TEST_F(BrowserTest, CloseWithAppMenuOpen) {
  if (browser_defaults::kBrowserAliveWithNoWindows)
    return;

  // We need a message loop running for menus on windows.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&RunCloseWithAppMenuCallback, browser()));
}

#if !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(BrowserTest, OpenAppWindowLikeNtp) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load an app
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app/")));
  const Extension* extension_app = GetExtension();

  // Launch it in a window, as AppLauncherHandler::HandleLaunchApp() would.
  WebContents* app_window = OpenApplication(AppLaunchParams(
      browser()->profile(), extension_app, extensions::LAUNCH_CONTAINER_WINDOW,
      WindowOpenDisposition::NEW_WINDOW, extensions::SOURCE_TEST));
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
  Browser* new_browser = NULL;
  for (auto* b : *BrowserList::GetInstance()) {
    if (b != browser())
      new_browser = b;
  }
  ASSERT_TRUE(new_browser);
  ASSERT_TRUE(new_browser != browser());

  EXPECT_TRUE(new_browser->is_app());

  // The browser's app name should include the extension's id.
  std::string app_name = new_browser->app_name_;
  EXPECT_NE(app_name.find(extension_app->id()), std::string::npos)
      << "Name " << app_name << " should contain id "<< extension_app->id();
}
#endif  // !defined(OS_MACOSX)

// Makes sure the browser doesn't crash when
// set_show_state(ui::SHOW_STATE_MAXIMIZED) has been invoked.
IN_PROC_BROWSER_TEST_F(BrowserTest, StartMaximized) {
  Browser::Type types[] = { Browser::TYPE_TABBED, Browser::TYPE_POPUP };
  for (size_t i = 0; i < arraysize(types); ++i) {
    Browser::CreateParams params(types[i], browser()->profile(), true);
    params.initial_show_state = ui::SHOW_STATE_MAXIMIZED;
    AddBlankTabAndShow(new Browser(params));
  }
}

// Makes sure the browser doesn't crash when
// set_show_state(ui::SHOW_STATE_MINIMIZED) has been invoked.
IN_PROC_BROWSER_TEST_F(BrowserTest, StartMinimized) {
  Browser::Type types[] = { Browser::TYPE_TABBED, Browser::TYPE_POPUP };
  for (size_t i = 0; i < arraysize(types); ++i) {
    Browser::CreateParams params(types[i], browser()->profile(), true);
    params.initial_show_state = ui::SHOW_STATE_MINIMIZED;
    AddBlankTabAndShow(new Browser(params));
  }
}

// Makes sure the forward button is disabled immediately when navigating
// forward to a slow-to-commit page.
IN_PROC_BROWSER_TEST_F(BrowserTest, ForwardDisabledOnForward) {
  GURL blank_url(url::kAboutBlankURL);
  ui_test_utils::NavigateToURL(browser(), blank_url);

  ui_test_utils::NavigateToURL(
      browser(), ui_test_utils::GetTestUrl(
                     base::FilePath(base::FilePath::kCurrentDirectory),
                     base::FilePath(kTitle1File)));

  content::WindowedNotificationObserver back_nav_load_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &browser()->tab_strip_model()->GetActiveWebContents()->
              GetController()));
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  back_nav_load_observer.Wait();
  CommandUpdater* command_updater = browser()->command_controller();
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_FORWARD));

  content::WindowedNotificationObserver forward_nav_load_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &browser()->tab_strip_model()->GetActiveWebContents()->
              GetController()));
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
  IncognitoModePrefs::SetAvailability(browser()->profile()->GetPrefs(),
                                      IncognitoModePrefs::FORCED);
  // Bookmarks & Settings commands should get disabled.
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_NEW_WINDOW));
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_SHOW_BOOKMARK_MANAGER));
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_IMPORT_SETTINGS));
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_MANAGE_EXTENSIONS));
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_OPTIONS));
  // New Incognito Window command, however, should be enabled.
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_NEW_INCOGNITO_WINDOW));

  // Create a new browser.
  Browser* new_browser = new Browser(Browser::CreateParams(
      browser()->profile()->GetOffTheRecordProfile(), true));
  CommandUpdater* new_command_updater = new_browser->command_controller();
  // It should have Bookmarks & Settings commands disabled by default.
  EXPECT_FALSE(new_command_updater->IsCommandEnabled(IDC_NEW_WINDOW));
  EXPECT_FALSE(new_command_updater->IsCommandEnabled(
      IDC_SHOW_BOOKMARK_MANAGER));
  EXPECT_FALSE(new_command_updater->IsCommandEnabled(IDC_IMPORT_SETTINGS));
  EXPECT_FALSE(new_command_updater->IsCommandEnabled(IDC_MANAGE_EXTENSIONS));
  EXPECT_FALSE(new_command_updater->IsCommandEnabled(IDC_OPTIONS));
  EXPECT_TRUE(new_command_updater->IsCommandEnabled(IDC_NEW_INCOGNITO_WINDOW));
}

// Makes sure New Incognito Window command is disabled when Incognito mode is
// not available.
IN_PROC_BROWSER_TEST_F(BrowserTest,
                       NoNewIncognitoWindowWhenIncognitoIsDisabled) {
  CommandUpdater* command_updater = browser()->command_controller();
  // Set Incognito to DISABLED.
  IncognitoModePrefs::SetAvailability(browser()->profile()->GetPrefs(),
                                      IncognitoModePrefs::DISABLED);
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
      new Browser(Browser::CreateParams(browser()->profile(), true));
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
  BrowserTestWithExtensionsDisabled() {}
  ~BrowserTestWithExtensionsDisabled() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    BrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableExtensions);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserTestWithExtensionsDisabled);
};

// Makes sure Extensions and Settings commands are disabled in certain
// circumstances even though normally they should stay enabled.
IN_PROC_BROWSER_TEST_F(BrowserTestWithExtensionsDisabled,
                       DisableExtensionsAndSettingsWhenIncognitoIsDisabled) {
  CommandUpdater* command_updater = browser()->command_controller();
  // Set Incognito to DISABLED.
  IncognitoModePrefs::SetAvailability(browser()->profile()->GetPrefs(),
                                      IncognitoModePrefs::DISABLED);
  // Make sure Manage Extensions command is disabled.
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_MANAGE_EXTENSIONS));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_NEW_WINDOW));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SHOW_BOOKMARK_MANAGER));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_IMPORT_SETTINGS));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_OPTIONS));

  // Create a popup (non-main-UI-type) browser. Settings command as well
  // as Extensions should be disabled.
  Browser* popup_browser = new Browser(
      Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile(), true));
  CommandUpdater* popup_command_updater = popup_browser->command_controller();
  EXPECT_FALSE(popup_command_updater->IsCommandEnabled(IDC_MANAGE_EXTENSIONS));
  EXPECT_FALSE(popup_command_updater->IsCommandEnabled(IDC_OPTIONS));
  EXPECT_TRUE(popup_command_updater->IsCommandEnabled(
      IDC_SHOW_BOOKMARK_MANAGER));
  EXPECT_FALSE(popup_command_updater->IsCommandEnabled(IDC_IMPORT_SETTINGS));
}

// Makes sure Extensions and Settings commands are disabled in certain
// circumstances even though normally they should stay enabled.
IN_PROC_BROWSER_TEST_F(BrowserTest,
                       DisableOptionsAndImportMenuItemsConsistently) {
  // Create a popup browser.
  Browser* popup_browser = new Browser(
      Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile(), true));
  CommandUpdater* command_updater = popup_browser->command_controller();
  // OPTIONS and IMPORT_SETTINGS are disabled for a non-normal UI.
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_OPTIONS));
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_IMPORT_SETTINGS));

  // Set Incognito to FORCED.
  IncognitoModePrefs::SetAvailability(popup_browser->profile()->GetPrefs(),
                                      IncognitoModePrefs::FORCED);
  // OPTIONS and IMPORT_SETTINGS are disabled when Incognito is forced.
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_OPTIONS));
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_IMPORT_SETTINGS));
  // Set Incognito to AVAILABLE.
  IncognitoModePrefs::SetAvailability(popup_browser->profile()->GetPrefs(),
                                      IncognitoModePrefs::ENABLED);
  // OPTIONS and IMPORT_SETTINGS are still disabled since it is a non-normal UI.
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_OPTIONS));
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_IMPORT_SETTINGS));
}

namespace {

void OnZoomLevelChanged(const base::Closure& callback,
                        const HostZoomMap::ZoomLevelChange& host) {
  callback.Run();
}

}  // namespace

#if defined(OS_WIN)
// Flakes regularly on Windows XP
// http://crbug.com/146040
#define MAYBE_PageZoom DISABLED_PageZoom
#else
#define MAYBE_PageZoom PageZoom
#endif

namespace {

int GetZoomPercent(const content::WebContents* contents,
                   bool* enable_plus,
                   bool* enable_minus) {
  int percent =
      zoom::ZoomController::FromWebContents(contents)->GetZoomPercent();
  *enable_plus = percent < contents->GetMaximumZoomPercent();
  *enable_minus = percent > contents->GetMinimumZoomPercent();
  return percent;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(BrowserTest, MAYBE_PageZoom) {
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  bool enable_plus, enable_minus;

  {
    scoped_refptr<content::MessageLoopRunner> loop_runner(
        new content::MessageLoopRunner);
    content::HostZoomMap::ZoomLevelChangedCallback callback(
        base::Bind(&OnZoomLevelChanged, loop_runner->QuitClosure()));
    std::unique_ptr<content::HostZoomMap::Subscription> sub =
        content::HostZoomMap::GetDefaultForBrowserContext(browser()->profile())
            ->AddZoomLevelChangedCallback(callback);
    chrome::Zoom(browser(), content::PAGE_ZOOM_IN);
    loop_runner->Run();
    sub.reset();
    EXPECT_EQ(GetZoomPercent(contents, &enable_plus, &enable_minus), 110);
    EXPECT_TRUE(enable_plus);
    EXPECT_TRUE(enable_minus);
  }

  {
    scoped_refptr<content::MessageLoopRunner> loop_runner(
        new content::MessageLoopRunner);
    content::HostZoomMap::ZoomLevelChangedCallback callback(
        base::Bind(&OnZoomLevelChanged, loop_runner->QuitClosure()));
    std::unique_ptr<content::HostZoomMap::Subscription> sub =
        content::HostZoomMap::GetDefaultForBrowserContext(browser()->profile())
            ->AddZoomLevelChangedCallback(callback);
    chrome::Zoom(browser(), content::PAGE_ZOOM_RESET);
    loop_runner->Run();
    sub.reset();
    EXPECT_EQ(GetZoomPercent(contents, &enable_plus, &enable_minus), 100);
    EXPECT_TRUE(enable_plus);
    EXPECT_TRUE(enable_minus);
  }

  {
    scoped_refptr<content::MessageLoopRunner> loop_runner(
        new content::MessageLoopRunner);
    content::HostZoomMap::ZoomLevelChangedCallback callback(
        base::Bind(&OnZoomLevelChanged, loop_runner->QuitClosure()));
    std::unique_ptr<content::HostZoomMap::Subscription> sub =
        content::HostZoomMap::GetDefaultForBrowserContext(browser()->profile())
            ->AddZoomLevelChangedCallback(callback);
    chrome::Zoom(browser(), content::PAGE_ZOOM_OUT);
    loop_runner->Run();
    sub.reset();
    EXPECT_EQ(GetZoomPercent(contents, &enable_plus, &enable_minus), 90);
    EXPECT_TRUE(enable_plus);
    EXPECT_TRUE(enable_minus);
  }

  chrome::Zoom(browser(), content::PAGE_ZOOM_RESET);
}

IN_PROC_BROWSER_TEST_F(BrowserTest, InterstitialCommandDisable) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/empty.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  CommandUpdater* command_updater = browser()->command_controller();
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_VIEW_SOURCE));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_PRINT));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SAVE_PAGE));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_DUPLICATE_TAB));

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();

  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents, false, GURL());
  content::WaitForInterstitialAttach(contents);

  EXPECT_TRUE(contents->ShowingInterstitialPage());

  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_VIEW_SOURCE));
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_PRINT));
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_SAVE_PAGE));
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_DUPLICATE_TAB));

  // Proceed and wait for interstitial to detach. This doesn't destroy
  // |contents|.
  interstitial->Proceed();
  content::WaitForInterstitialDetach(contents);
  // interstitial is deleted now.

  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_VIEW_SOURCE));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_PRINT));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SAVE_PAGE));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_DUPLICATE_TAB));
}

// Ensure that creating an interstitial page closes any JavaScript dialogs
// that were present on the previous page.  See http://crbug.com/295695.
IN_PROC_BROWSER_TEST_F(BrowserTest, InterstitialClosesDialogs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/empty.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  JavaScriptDialogTabHelper* js_helper =
      JavaScriptDialogTabHelper::FromWebContents(contents);
  base::RunLoop dialog_wait;
  js_helper->SetDialogShownCallbackForTesting(dialog_wait.QuitClosure());
  contents->GetMainFrame()->ExecuteJavaScriptForTests(
      ASCIIToUTF16("alert('Dialog showing!');"));
  dialog_wait.Run();
  EXPECT_TRUE(js_helper->IsShowingDialogForTesting());

  TestInterstitialPage* interstitial =
      new TestInterstitialPage(contents, false, GURL());
  content::WaitForInterstitialAttach(contents);

  // The interstitial should have closed the dialog.
  EXPECT_TRUE(contents->ShowingInterstitialPage());
  EXPECT_FALSE(js_helper->IsShowingDialogForTesting());

  // Don't proceed and wait for interstitial to detach. This doesn't destroy
  // |contents|.
  interstitial->DontProceed();
  content::WaitForInterstitialDetach(contents);
  // interstitial is deleted now.

  // Make sure input events still work in the renderer process.
  EXPECT_FALSE(contents->GetMainFrame()->GetProcess()->IgnoreInputEvents());
}


IN_PROC_BROWSER_TEST_F(BrowserTest, InterstitialCloseTab) {
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();

  // Interstitial will delete itself when we close the tab.
  new TestInterstitialPage(contents, false, GURL());
  content::WaitForInterstitialAttach(contents);

  EXPECT_TRUE(contents->ShowingInterstitialPage());

  // Close the tab and wait for interstitial detach. This destroys |contents|.
  content::RunTaskAndWaitForInterstitialDetach(
      contents, base::Bind(&chrome::CloseTab, browser()));
  // interstitial is deleted now.
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
  Browser* popup_browser = new Browser(
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
  Browser* app_browser = new Browser(Browser::CreateParams::CreateForApp(
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
  Browser* app_popup_browser = new Browser(Browser::CreateParams::CreateForApp(
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

// Flaky on Chrome OS only. TODO(https://crbug.com/823043) fix it.
#if defined(OS_CHROMEOS)
#define MAYBE_WindowOpenClose1 DISABLED_WindowOpenClose1
#else
#define MAYBE_WindowOpenClose1 WindowOpenClose1
#endif
IN_PROC_BROWSER_TEST_F(BrowserTest, MAYBE_WindowOpenClose1) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisablePopupBlocking);
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/window.close.html");
  GURL::Replacements add_query;
  std::string query("test1");
  add_query.SetQuery(query.c_str(), url::Component(0, query.length()));
  url = url.ReplaceComponents(add_query);

  base::string16 title = ASCIIToUTF16("Title Of Awesomeness");
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), title);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url, 2);
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(BrowserTest, WindowOpenClose2) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisablePopupBlocking);
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/window.close.html");
  GURL::Replacements add_query;
  std::string query("test2");
  add_query.SetQuery(query.c_str(), url::Component(0, query.length()));
  url = url.ReplaceComponents(add_query);

  base::string16 title = ASCIIToUTF16("Title Of Awesomeness");
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), title);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url, 2);
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
}

#if (defined(OS_WIN) && !defined(NDEBUG))
// Times out on windows (dbg). https://crbug.com/753691.
#define MAYBE_WindowOpenClose3 DISABLED_WindowOpenClose3
#else
#define MAYBE_WindowOpenClose3 WindowOpenClose3
#endif
IN_PROC_BROWSER_TEST_F(BrowserTest, MAYBE_WindowOpenClose3) {
#if defined(OS_MACOSX)
  // Ensure that tests don't wait for frames that will never come.
  ui::CATransactionCoordinator::Get().DisableForTesting();
#endif
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisablePopupBlocking);
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/window.close.html");
  GURL::Replacements add_query;
  std::string query("test3");
  add_query.SetQuery(query.c_str(), url::Component(0, query.length()));
  url = url.ReplaceComponents(add_query);

  base::string16 title = ASCIIToUTF16("Title Of Awesomeness");
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), title);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url, 2);
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
}

// TODO(linux_aura) http://crbug.com/163931
// Mac disabled: http://crbug.com/169820
#if !defined(OS_MACOSX) && !(defined(OS_LINUX) && !defined(OS_CHROMEOS))
IN_PROC_BROWSER_TEST_F(BrowserTest, FullscreenBookmarkBar) {
  chrome::ToggleBookmarkBar(browser());
  EXPECT_EQ(BookmarkBar::SHOW, browser()->bookmark_bar_state());
  chrome::ToggleFullscreenMode(browser());
  EXPECT_TRUE(browser()->window()->IsFullscreen());
#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
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

  base::string16 expected_title(ASCIIToUTF16("Disallowed"));
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
  title_watcher.AlsoWaitForTitle(ASCIIToUTF16("Allowed"));
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

class KioskModeTest : public BrowserTest {
 public:
  KioskModeTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kKioskMode);
  }
};

#if defined(OS_MACOSX) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
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

#if defined(OS_WIN)
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
  ASSERT_EQ(1u, g_browser_process->profile_manager()->
      GetProfileAttributesStorage().GetNumberOfProfiles());
}
#endif  // defined(OS_WIN)

#if defined(OS_WIN)
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
  ASSERT_EQ(1u, g_browser_process->profile_manager()->
      GetProfileAttributesStorage().GetNumberOfProfiles());
}
#endif  // defined(OS_WIN)

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

  ui_test_utils::BrowserAddedObserver browser_added_observer;
  chrome::NewEmptyWindow(profile);
  browser_added_observer.WaitForSingleNewBrowser();

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
      sessions::BaseSessionService* base_session_service) {
    sessions::BaseSessionServiceTestHelper test_helper(base_session_service);
    return test_helper.ProcessedAnyCommands();
  }
};

IN_PROC_BROWSER_TEST_F(NoStartupWindowTest, NoStartupWindowBasicTest) {
  // No browser window should be started by default.
  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());

  // Starting a browser window should work just fine.
  ui_test_utils::BrowserAddedObserver browser_added_observer;
  CreateBrowser(ProfileManager::GetActiveUserProfile());
  browser_added_observer.WaitForSingleNewBrowser();

  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
}

// Chromeos needs to track app windows because it considers them to be part of
// session state.
#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(NoStartupWindowTest, DontInitSessionServiceForApps) {
  Profile* profile = ProfileManager::GetActiveUserProfile();

  SessionService* session_service =
      SessionServiceFactory::GetForProfile(profile);
  sessions::BaseSessionService* base_session_service =
      session_service->GetBaseSessionServiceForTest();
  ASSERT_FALSE(ProcessedAnyCommands(base_session_service));

  ui_test_utils::BrowserAddedObserver browser_added_observer;
  CreateBrowserForApp("blah", profile);
  browser_added_observer.WaitForSingleNewBrowser();

  ASSERT_FALSE(ProcessedAnyCommands(base_session_service));
}
#endif  // !defined(OS_CHROMEOS)

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
  EXPECT_TRUE(browser()->is_app());
}

// Confirm chrome://version contains some expected content.
IN_PROC_BROWSER_TEST_F(BrowserTest, AboutVersion) {
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIVersionURL));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_GT(ui_test_utils::FindInPage(tab, ASCIIToUTF16("WebKit"), true, true,
                                      NULL, NULL),
            0);
  ASSERT_GT(ui_test_utils::FindInPage(tab, ASCIIToUTF16("OS"), true, true,
                                      NULL, NULL),
            0);
  ASSERT_GT(ui_test_utils::FindInPage(tab, ASCIIToUTF16("JavaScript"), true,
                                      true, NULL, NULL),
            0);
  ASSERT_GT(ui_test_utils::FindInPage(tab, ASCIIToUTF16("Flash"), true,
                                      true, NULL, NULL),
            0);
}

static const base::FilePath::CharType* kTestDir =
    FILE_PATH_LITERAL("click_modifier");
static const char kFirstPageTitle[] = "First window";
static const char kSecondPageTitle[] = "New window!";

class ClickModifierTest : public InProcessBrowserTest {
 public:
  ClickModifierTest() {
  }

  // Returns a url that opens a new window or tab when clicked, via javascript.
  GURL GetWindowOpenURL() {
    return ui_test_utils::GetTestUrl(
      base::FilePath(kTestDir),
      base::FilePath(FILE_PATH_LITERAL("window_open.html")));
  }

  // Returns a url that follows a simple link when clicked, unless affected by
  // modifiers.
  GURL GetHrefURL() {
    return ui_test_utils::GetTestUrl(
      base::FilePath(kTestDir),
      base::FilePath(FILE_PATH_LITERAL("href.html")));
  }

  base::string16 GetFirstPageTitle() {
    return ASCIIToUTF16(kFirstPageTitle);
  }

  base::string16 GetSecondPageTitle() {
    return ASCIIToUTF16(kSecondPageTitle);
  }

  // Loads our test page and simulates a single click using the supplied button
  // and modifiers.  The click will cause either a navigation or the creation of
  // a new window or foreground or background tab.  We verify that the expected
  // disposition occurs.
  void RunTest(Browser* browser,
               const GURL& url,
               int modifiers,
               blink::WebMouseEvent::Button button,
               WindowOpenDisposition disposition) {
    ui_test_utils::NavigateToURL(browser, url);
    EXPECT_EQ(1u, chrome::GetBrowserCount(browser->profile()));
    EXPECT_EQ(1, browser->tab_strip_model()->count());
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(url, web_contents->GetURL());

    if (disposition == WindowOpenDisposition::CURRENT_TAB) {
      content::WebContents* web_contents =
          browser->tab_strip_model()->GetActiveWebContents();
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

 private:
  DISALLOW_COPY_AND_ASSIGN(ClickModifierTest);
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
#if defined(OS_MACOSX)
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
#if defined(OS_MACOSX)
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
#if defined(OS_MACOSX)
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
// http://crbug.com/396347
IN_PROC_BROWSER_TEST_F(ClickModifierTest, DISABLED_HrefControlShiftClickTest) {
#if defined(OS_MACOSX)
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
// http://crbug.com/396347
IN_PROC_BROWSER_TEST_F(ClickModifierTest, DISABLED_HrefShiftMiddleClickTest) {
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
  https_test_server.ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));
  ASSERT_TRUE(https_test_server.Start());

  // Start with NTP.
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab"));
  ASSERT_EQ(BookmarkBar::DETACHED, browser()->bookmark_bar_state());
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderViewHost* prev_rvh = web_contents->GetRenderViewHost();
  const int height_inset =
      browser()->window()->GetRenderViewHeightInsetWithDetachedBookmarkBar();
  const gfx::Size initial_wcv_size =
      web_contents->GetContainerBounds().size();
  RenderViewSizeObserver observer(web_contents, browser()->window());

  // Navigate to a non-NTP page, without resizing WebContentsView.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  ASSERT_EQ(BookmarkBar::HIDDEN, browser()->bookmark_bar_state());
  // A new RenderViewHost should be created.
  EXPECT_NE(prev_rvh, web_contents->GetRenderViewHost());
  prev_rvh = web_contents->GetRenderViewHost();
  gfx::Size rwhv_create_size0, rwhv_commit_size0, wcv_commit_size0;
  observer.GetSizeForRenderViewHost(web_contents->GetRenderViewHost(),
                                    &rwhv_create_size0,
                                    &rwhv_commit_size0,
                                    &wcv_commit_size0);
  // The create height of RenderWidgetHostView should include the height inset.
  EXPECT_EQ(gfx::Size(initial_wcv_size.width(),
                      initial_wcv_size.height() + height_inset),
            rwhv_create_size0);
  // When a navigation entry is committed, the size of RenderWidgetHostView
  // should be the same as when it was first created.
  EXPECT_EQ(rwhv_create_size0, rwhv_commit_size0);
  // Sizes of the current RenderWidgetHostView and WebContentsView should not
  // change before and after WebContentsDelegate::DidNavigateMainFramePostCommit
  // (implemented by Browser); we obtain the sizes before PostCommit via
  // WebContentsObserver::NavigationEntryCommitted (implemented by
  // RenderViewSizeObserver).
  EXPECT_EQ(rwhv_commit_size0,
            web_contents->GetRenderWidgetHostView()->GetViewBounds().size());
// The behavior differs between OSX and views.
// In OSX, the wcv does not change size until after the commit, when the
// bookmark bar disappears (correct).
// In views, the wcv changes size at commit time.
#if defined(OS_MACOSX)
  EXPECT_EQ(gfx::Size(wcv_commit_size0.width(),
                      wcv_commit_size0.height() + height_inset),
            web_contents->GetContainerBounds().size());
#else
  EXPECT_EQ(wcv_commit_size0, web_contents->GetContainerBounds().size());
#endif

  // Navigate to another non-NTP page, without resizing WebContentsView.
  ui_test_utils::NavigateToURL(browser(),
                               https_test_server.GetURL("/title2.html"));
  ASSERT_EQ(BookmarkBar::HIDDEN, browser()->bookmark_bar_state());
  // A new RenderVieHost should be created.
  EXPECT_NE(prev_rvh, web_contents->GetRenderViewHost());
  gfx::Size rwhv_create_size1, rwhv_commit_size1, wcv_commit_size1;
  observer.GetSizeForRenderViewHost(web_contents->GetRenderViewHost(),
                                    &rwhv_create_size1,
                                    &rwhv_commit_size1,
                                    &wcv_commit_size1);
  EXPECT_EQ(rwhv_create_size1, rwhv_commit_size1);
  EXPECT_EQ(rwhv_commit_size1,
            web_contents->GetRenderWidgetHostView()->GetViewBounds().size());
  EXPECT_EQ(wcv_commit_size1, web_contents->GetContainerBounds().size());

  // Navigate from NTP to a non-NTP page, resizing WebContentsView while
  // navigation entry is pending.
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab"));
  gfx::Size wcv_resize_insets(1, 1);
  observer.set_wcv_resize_insets(wcv_resize_insets);
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title2.html"));
  ASSERT_EQ(BookmarkBar::HIDDEN, browser()->bookmark_bar_state());
  gfx::Size rwhv_create_size2, rwhv_commit_size2, wcv_commit_size2;
  observer.GetSizeForRenderViewHost(web_contents->GetRenderViewHost(),
                                    &rwhv_create_size2,
                                    &rwhv_commit_size2,
                                    &wcv_commit_size2);

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

  // The create height of RenderWidgetHostView should include the height inset.
  EXPECT_EQ(gfx::Size(initial_wcv_size.width(),
                      initial_wcv_size.height() + height_inset),
            rwhv_create_size2);
  gfx::Size exp_commit_size(initial_wcv_size);

#if defined(OS_MACOSX)
  exp_commit_size.Enlarge(wcv_resize_insets.width(),
                          wcv_resize_insets.height());
#else
  exp_commit_size.Enlarge(wcv_resize_insets.width(),
                          wcv_resize_insets.height() + height_inset);
#endif
  EXPECT_EQ(exp_commit_size, rwhv_commit_size2);
  EXPECT_EQ(exp_commit_size, wcv_commit_size2);

  gfx::Size exp_final_size(initial_wcv_size);
  exp_final_size.Enlarge(wcv_resize_insets.width(),
                         wcv_resize_insets.height() + height_inset);
  EXPECT_EQ(exp_final_size,
            web_contents->GetRenderWidgetHostView()->GetViewBounds().size());
  EXPECT_EQ(exp_final_size, web_contents->GetContainerBounds().size());
}

IN_PROC_BROWSER_TEST_F(BrowserTest, CanDuplicateTab) {
  GURL url(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kTitle1File)));
  ui_test_utils::NavigateToURL(browser(), url);

  AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED);

  int active_index = browser()->tab_strip_model()->active_index();
  EXPECT_EQ(0, active_index);

  EXPECT_TRUE(chrome::CanDuplicateTab(browser()));
  EXPECT_TRUE(chrome::CanDuplicateTabAt(browser(), 0));
  EXPECT_TRUE(chrome::CanDuplicateTabAt(browser(), 1));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  TestInterstitialPage* interstitial =
      new TestInterstitialPage(web_contents, false, GURL());
  content::WaitForInterstitialAttach(web_contents);

  EXPECT_TRUE(web_contents->ShowingInterstitialPage());

  // Verify that the "Duplicate tab" command is disabled on interstitial
  // pages. Regression test for crbug.com/310812
  EXPECT_FALSE(chrome::CanDuplicateTab(browser()));
  EXPECT_FALSE(chrome::CanDuplicateTabAt(browser(), 0));
  EXPECT_TRUE(chrome::CanDuplicateTabAt(browser(), 1));

  // Don't proceed and wait for interstitial to detach. This doesn't
  // destroy |contents|.
  interstitial->DontProceed();
  content::WaitForInterstitialDetach(web_contents);
  // interstitial is deleted now.

  EXPECT_TRUE(chrome::CanDuplicateTab(browser()));
  EXPECT_TRUE(chrome::CanDuplicateTabAt(browser(), 0));
  EXPECT_TRUE(chrome::CanDuplicateTabAt(browser(), 1));
}

IN_PROC_BROWSER_TEST_F(BrowserTest, DefaultMediaDevices) {
  const std::string kDefaultAudioCapture1 = "test_default_audio_capture";
  const std::string kDefaultVideoCapture1 = "test_default_video_capture";
  auto SetString = [this](const std::string& path, const std::string& value) {
    browser()->profile()->GetPrefs()->SetString(path, value);
  };
  SetString(prefs::kDefaultAudioCaptureDevice, kDefaultAudioCapture1);
  SetString(prefs::kDefaultVideoCaptureDevice, kDefaultVideoCapture1);

  ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab"));
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto GetDeviceID = [web_contents](content::MediaStreamType type) {
    return web_contents->GetDelegate()->GetDefaultMediaDeviceID(web_contents,
                                                                type);
  };
  EXPECT_EQ(kDefaultAudioCapture1,
            GetDeviceID(content::MEDIA_DEVICE_AUDIO_CAPTURE));
  EXPECT_EQ(kDefaultVideoCapture1,
            GetDeviceID(content::MEDIA_DEVICE_VIDEO_CAPTURE));

  const std::string kDefaultAudioCapture2 = "test_default_audio_capture_2";
  const std::string kDefaultVideoCapture2 = "test_default_video_capture_2";
  SetString(prefs::kDefaultAudioCaptureDevice, kDefaultAudioCapture2);
  SetString(prefs::kDefaultVideoCaptureDevice, kDefaultVideoCapture2);
  EXPECT_EQ(kDefaultAudioCapture2,
            GetDeviceID(content::MEDIA_DEVICE_AUDIO_CAPTURE));
  EXPECT_EQ(kDefaultVideoCapture2,
            GetDeviceID(content::MEDIA_DEVICE_VIDEO_CAPTURE));
}

namespace {
class JSBooleanResultGetter {
 public:
  JSBooleanResultGetter() = default;
  void OnJsExecutionDone(base::Closure callback, const base::Value* value) {
    js_result_.reset(value->DeepCopy());
    callback.Run();
  }
  bool GetResult() const {
    bool res;
    CHECK(js_result_);
    CHECK(js_result_->GetAsBoolean(&res));
    return res;
  }

 private:
  std::unique_ptr<base::Value> js_result_;
  DISALLOW_COPY_AND_ASSIGN(JSBooleanResultGetter);
};

void CheckDisplayModeMQ(
    const base::string16& display_mode,
    content::WebContents* web_contents) {
  base::string16 funtcion =
      ASCIIToUTF16("(function() {return window.matchMedia('(display-mode: ") +
      display_mode + ASCIIToUTF16(")').matches;})();");
  JSBooleanResultGetter js_result_getter;
  // Execute the JS to run the tests, and wait until it has finished.
  base::RunLoop run_loop;
  web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
      funtcion,
      base::Bind(&JSBooleanResultGetter::OnJsExecutionDone,
          base::Unretained(&js_result_getter), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(js_result_getter.GetResult());
}

}  // namespace

// flaky new test: http://crbug.com/471703
IN_PROC_BROWSER_TEST_F(BrowserTest, DISABLED_ChangeDisplayMode) {
  CheckDisplayModeMQ(
      ASCIIToUTF16("browser"),
      browser()->tab_strip_model()->GetActiveWebContents());

  Profile* profile = ProfileManager::GetActiveUserProfile();
  ui_test_utils::BrowserAddedObserver browser_added_observer;
  Browser* app_browser = CreateBrowserForApp("blah", profile);
  browser_added_observer.WaitForSingleNewBrowser();
  auto* app_contents = app_browser->tab_strip_model()->GetActiveWebContents();
  CheckDisplayModeMQ(ASCIIToUTF16("standalone"), app_contents);

  app_browser->exclusive_access_manager()->context()->EnterFullscreen(
      GURL(), EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION);

  // Sync navigation just to make sure IPC has passed (updated
  // display mode is delivered to RP).
  content::TestNavigationObserver observer(app_contents, 1);
  ui_test_utils::NavigateToURL(app_browser, GURL(url::kAboutBlankURL));
  observer.Wait();

  CheckDisplayModeMQ(ASCIIToUTF16("fullscreen"), app_contents);
}

#if defined(OS_MACOSX)
// The size computation on popups is wrong in MacViews, https://crbug.com/834908
#define MAYBE_TestPopupBounds DISABLED_TestPopupBounds
#else
#define MAYBE_TestPopupBounds TestPopupBounds
#endif

// Test to ensure the bounds of popup, devtool, and app windows are properly
// restored.
IN_PROC_BROWSER_TEST_F(BrowserTest, MAYBE_TestPopupBounds) {
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
    Browser* browser = new Browser(params);
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
    Browser* browser = new Browser(params);
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
    Browser* browser = new Browser(params);
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
    Browser* browser = new Browser(params);
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
    Browser* browser = new Browser(params);
    gfx::Rect bounds = browser->window()->GetBounds();

    // Should be EXPECT_EQ, but this width is inconsistent across platforms.
    // See https://crbug.com/567925.
    EXPECT_GE(bounds.width(), 100);
    EXPECT_EQ(122, bounds.height());
    browser->window()->Close();
  }
}

// Makes sure showing dialogs drops fullscreen.
IN_PROC_BROWSER_TEST_F(BrowserTest, DialogsDropFullscreen) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  content::WebContentsDelegate* browser_as_wc_delegate =
      static_cast<content::WebContentsDelegate*>(browser());
  web_modal::WebContentsModalDialogManagerDelegate* browser_as_dialog_delegate =
      static_cast<web_modal::WebContentsModalDialogManagerDelegate*>(browser());

  // Simulate the tab requesting fullscreen.
  browser_as_wc_delegate->EnterFullscreenModeForTab(
      tab, GURL(), blink::WebFullscreenOptions());
  EXPECT_TRUE(browser_as_wc_delegate->IsFullscreenForTabOrPending(tab));

  // The tab gets a modal dialog.
  browser_as_dialog_delegate->SetWebContentsBlocked(tab, true);

  // The dialog should drop fullscreen.
  EXPECT_FALSE(browser_as_wc_delegate->IsFullscreenForTabOrPending(tab));

  browser_as_dialog_delegate->SetWebContentsBlocked(tab, false);
}

// Makes sure showing dialogs does NOT drop fullscreen when the browser is in
// FullscreenWithinTab mode. This is an exception to the primary behavior tested
// by BrowserTest.DialogsDropFullscreen above. See "FullscreenWithinTab note" in
// FullscreenController's class-level comments for further details.
IN_PROC_BROWSER_TEST_F(BrowserTest, DialogsAllowedInFullscreenWithinTabMode) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  content::WebContentsDelegate* browser_as_wc_delegate =
      static_cast<content::WebContentsDelegate*>(browser());
  web_modal::WebContentsModalDialogManagerDelegate* browser_as_dialog_delegate =
      static_cast<web_modal::WebContentsModalDialogManagerDelegate*>(browser());

  // Simulate a screen-captured tab requesting fullscreen.
  tab->IncrementCapturerCount(gfx::Size(1280, 720));
  browser_as_wc_delegate->EnterFullscreenModeForTab(
      tab, GURL(), blink::WebFullscreenOptions());
  EXPECT_TRUE(browser_as_wc_delegate->IsFullscreenForTabOrPending(tab));

  // The tab gets a modal dialog.
  browser_as_dialog_delegate->SetWebContentsBlocked(tab, true);

  // The dialog should NOT drop fullscreen.
  EXPECT_TRUE(browser_as_wc_delegate->IsFullscreenForTabOrPending(tab));

  browser_as_dialog_delegate->SetWebContentsBlocked(tab, false);
  tab->DecrementCapturerCount();
}
