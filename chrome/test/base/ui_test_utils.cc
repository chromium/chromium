// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ui_test_utils.h"

#include <stddef.h>

#include <memory>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/omnibox/autocomplete_controller_emitter_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/find_result_waiter.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/download/public/common/download_item.h"
#include "components/find_in_page/find_notification_details.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/javascript_dialogs/app_modal_dialog_controller.h"
#include "components/javascript_dialogs/app_modal_dialog_queue.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_controller_emitter.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/base/filename_util.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_util.h"
#include "services/device/public/mojom/geoposition.mojom.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/widget/widget_interactive_uitest_utils.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#if defined(TOOLKIT_VIEWS)
#include "ui/views/test/widget_test_api.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#endif

using content::NavigationController;
using content::NavigationEntry;
using content::OpenURLParams;
using content::Referrer;
using content::WebContents;

namespace ui_test_utils {

namespace {

Browser* WaitForBrowserNotInSet(std::set<Browser*> excluded_browsers) {
  Browser* new_browser = GetBrowserNotInSet(excluded_browsers);
  if (!new_browser) {
    new_browser = WaitForBrowserToOpen();
    // The new browser should never be in |excluded_browsers|.
    DCHECK(!base::Contains(excluded_browsers, new_browser));
  }
  return new_browser;
}

class AppModalDialogWaiter : public javascript_dialogs::AppModalDialogObserver {
 public:
  AppModalDialogWaiter() = default;
  AppModalDialogWaiter(const AppModalDialogWaiter&) = delete;
  AppModalDialogWaiter& operator=(const AppModalDialogWaiter&) = delete;
  ~AppModalDialogWaiter() override = default;

  javascript_dialogs::AppModalDialogController* Wait() {
    if (dialog_)
      return dialog_;
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
    EXPECT_TRUE(dialog_);
    return dialog_;
  }

  // AppModalDialogObserver:
  void Notify(javascript_dialogs::AppModalDialogController* dialog) override {
    DCHECK(!dialog_);
    dialog_ = dialog;
    CheckForHangMonitorDisabling(dialog);
    if (message_loop_runner_.get() && message_loop_runner_->loop_running())
      message_loop_runner_->Quit();
  }

  static void CheckForHangMonitorDisabling(
      javascript_dialogs::AppModalDialogController* dialog) {
    // If a test waits for a beforeunload dialog but hasn't disabled the
    // beforeunload hang timer before triggering it, there will be a race
    // between the dialog and the timer and the test will be flaky. We can't
    // disable the timer here, as it's too late, but we can tell when we've won
    // a race that we shouldn't have been in.
    if (!dialog->is_before_unload_dialog())
      return;

    // Unfortunately we don't know which frame spawned this dialog and should
    // have the hang monitor disabled, so we cheat a bit and search for *a*
    // frame with the hang monitor disabled. The failure case that's worrisome
    // is someone who doesn't know the requirement to disable the hang monitor,
    // and this will catch that case.
    auto* contents = dialog->web_contents();
    bool found_disabled_for_testing = false;
    contents->GetPrimaryMainFrame()->ForEachRenderFrameHostWithAction(
        [&found_disabled_for_testing](content::RenderFrameHost* frame) {
          if (frame->IsBeforeUnloadHangMonitorDisabledForTesting()) {
            found_disabled_for_testing = true;
            return content::RenderFrameHost::FrameIterationAction::kStop;
          }
          return content::RenderFrameHost::FrameIterationAction::kContinue;
        });

    ASSERT_TRUE(found_disabled_for_testing)
        << "If waiting for a beforeunload dialog, the beforeunload timer "
           "must be disabled on the spawning frame to avoid flakiness.";
  }

 private:
  raw_ptr<javascript_dialogs::AppModalDialogController> dialog_ = nullptr;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
};


class AutocompleteChangeObserver : public AutocompleteController::Observer {
 public:
  explicit AutocompleteChangeObserver(Profile* profile) {
    scoped_observation_.Observe(
        AutocompleteControllerEmitterFactory::GetForBrowserContext(profile));
  }

  AutocompleteChangeObserver(const AutocompleteChangeObserver&) = delete;
  AutocompleteChangeObserver& operator=(const AutocompleteChangeObserver&) =
      delete;
  ~AutocompleteChangeObserver() override = default;

  void Wait() { run_loop_.Run(); }

  // AutocompleteController::Observer:
  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override {
    if (run_loop_.running())
      run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
  base::ScopedObservation<AutocompleteControllerEmitter,
                          AutocompleteController::Observer>
      scoped_observation_{this};
};

// Helper class to notify AllTabsObserver that a WebContents has been destroyed.
class WebContentsDestructionObserver : public content::WebContentsObserver {
 public:
  WebContentsDestructionObserver(base::OnceClosure destruction_cb,
                                 content::WebContents* web_contents)
      : WebContentsObserver(web_contents),
        destruction_cb_(std::move(destruction_cb)) {}
  ~WebContentsDestructionObserver() override = default;

  // WebContentsObserver
  void WebContentsDestroyed() override { std::move(destruction_cb_).Run(); }

 private:
  base::OnceClosure destruction_cb_;
};

}  // namespace

bool GetCurrentTabTitle(const Browser* browser, std::u16string* title) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return false;
  NavigationEntry* last_entry = web_contents->GetController().GetActiveEntry();
  if (!last_entry)
    return false;
  title->assign(last_entry->GetTitleForDisplay());
  return true;
}

void NavigateToURL(NavigateParams* params) {
  Navigate(params);
  content::WaitForLoadStop(params->navigated_or_inserted_contents);
}

void NavigateToURLWithPost(Browser* browser, const GURL& url) {
  NavigateParams params(browser, url, ui::PAGE_TRANSITION_FORM_SUBMIT);

  std::string post_data("test=body");
  params.post_data = network::ResourceRequestBody::CreateFromBytes(
      post_data.data(), post_data.size());

  NavigateToURL(&params);
}

content::RenderFrameHost* NavigateToURL(Browser* browser, const GURL& url) {
  return NavigateToURLWithDisposition(browser, url,
                                      WindowOpenDisposition::CURRENT_TAB,
                                      BROWSER_TEST_WAIT_FOR_LOAD_STOP);
}

content::RenderFrameHost*
NavigateToURLWithDispositionBlockUntilNavigationsComplete(
    Browser* browser,
    const GURL& url,
    int number_of_navigations,
    WindowOpenDisposition disposition,
    int browser_test_flags) {
  TRACE_EVENT1("test",
               "ui_test_utils::"
               "NavigateToURLWithDispositionBlockUntilNavigationsComplete",
               "params", [&](perfetto::TracedValue context) {
                 // TODO(crbug.com/40751990): Replace this with passing more
                 // parameters to TRACE_EVENT directly when available.
                 auto dict = std::move(context).WriteDictionary();
                 dict.Add("url", url);
                 dict.Add("number_of_navigations", number_of_navigations);
                 dict.Add("disposition", disposition);
                 dict.Add("browser_test_flags", browser_test_flags);
               });
  TabStripModel* tab_strip = browser->tab_strip_model();
  if (disposition == WindowOpenDisposition::CURRENT_TAB &&
      tab_strip->GetActiveWebContents())
    content::WaitForLoadStop(tab_strip->GetActiveWebContents());
  content::TestNavigationObserver same_tab_observer(
      tab_strip->GetActiveWebContents(), number_of_navigations,
      content::MessageLoopRunner::QuitMode::DEFERRED,
      /*ignore_uncommitted_navigations=*/false);
  if (!blink::IsRendererDebugURL(url))
    same_tab_observer.set_expected_initial_url(url);

  std::set<Browser*> initial_browsers;
  for (Browser* initial_browser : *BrowserList::GetInstance()) {
    initial_browsers.insert(initial_browser);
  }

  AllBrowserTabAddedWaiter tab_added_waiter;

  WebContents* web_contents =
      browser->OpenURL(OpenURLParams(url, Referrer(), disposition,
                                     ui::PAGE_TRANSITION_TYPED, false),
                       /*navigation_handle_callback=*/{});
  if (browser_test_flags & BROWSER_TEST_WAIT_FOR_BROWSER)
    browser = WaitForBrowserNotInSet(initial_browsers);
  if (browser_test_flags & BROWSER_TEST_WAIT_FOR_TAB)
    tab_added_waiter.Wait();
  if (!(browser_test_flags & BROWSER_TEST_WAIT_FOR_LOAD_STOP)) {
    // Some other flag caused the wait prior to this.
    return nullptr;
  }
  if (disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB) {
    EXPECT_TRUE(web_contents)
        << " Unable to wait for navigation to \"" << url.spec()
        << "\" because the new tab is not available yet";
    if (!web_contents)
      return nullptr;
  } else if ((disposition == WindowOpenDisposition::CURRENT_TAB) ||
             (disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB) ||
             (disposition == WindowOpenDisposition::SINGLETON_TAB)) {
    // The tab we navigated should be the active one.
    EXPECT_EQ(web_contents, browser->tab_strip_model()->GetActiveWebContents());
  }
  if (disposition == WindowOpenDisposition::CURRENT_TAB) {
    same_tab_observer.Wait();
    return web_contents->GetPrimaryMainFrame();
  } else if (web_contents) {
    content::TestNavigationObserver observer(
        web_contents, number_of_navigations,
        content::MessageLoopRunner::QuitMode::DEFERRED,
        /*ignore_uncommitted_navigations=*/false);
    if (!blink::IsRendererDebugURL(url))
      observer.set_expected_initial_url(url);
    observer.Wait();
    return web_contents->GetPrimaryMainFrame();
  }
  EXPECT_TRUE(web_contents)
      << " Unable to wait for navigation to \"" << url.spec() << "\""
      << " because we can't get the tab contents";
  return nullptr;
}

content::RenderFrameHost* NavigateToURLWithDisposition(
    Browser* browser,
    const GURL& url,
    WindowOpenDisposition disposition,
    int browser_test_flags) {
  return NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser, url, 1, disposition, browser_test_flags);
}

content::RenderFrameHost* NavigateToURLBlockUntilNavigationsComplete(
    Browser* browser,
    const GURL& url,
    int number_of_navigations) {
  return NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser, url, number_of_navigations, WindowOpenDisposition::CURRENT_TAB,
      BROWSER_TEST_WAIT_FOR_LOAD_STOP);
}

base::FilePath GetTestFilePath(const base::FilePath& dir,
                               const base::FilePath& file) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &path);
  return path.Append(dir).Append(file);
}

GURL GetTestUrl(const base::FilePath& dir, const base::FilePath& file) {
  return net::FilePathToFileURL(GetTestFilePath(dir, file));
}

bool GetRelativeBuildDirectory(base::FilePath* build_dir) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  // This function is used to find the build directory so TestServer can serve
  // built files (nexes, etc).  TestServer expects a path relative to the source
  // root.
  base::FilePath exe_dir =
      base::CommandLine::ForCurrentProcess()->GetProgram().DirName();
  base::FilePath src_dir;
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_dir)) {
    return false;
  }

  // We must first generate absolute paths to SRC and EXE and from there
  // generate a relative path.
  if (!exe_dir.IsAbsolute())
    exe_dir = base::MakeAbsoluteFilePath(exe_dir);
  if (!src_dir.IsAbsolute())
    src_dir = base::MakeAbsoluteFilePath(src_dir);
  if (!exe_dir.IsAbsolute())
    return false;
  if (!src_dir.IsAbsolute())
    return false;

  size_t match, exe_size, src_size;

  // Determine point at which src and exe diverge.
  auto exe_parts = exe_dir.GetComponents();
  auto src_parts = src_dir.GetComponents();
  exe_size = exe_parts.size();
  src_size = src_parts.size();
  for (match = 0; match < exe_size && match < src_size; ++match) {
    if (exe_parts[match] != src_parts[match])
      break;
  }

  // Create a relative path.
  *build_dir = base::FilePath();
  for (size_t tmp_itr = match; tmp_itr < src_size; ++tmp_itr)
    *build_dir = build_dir->Append(FILE_PATH_LITERAL(".."));
  for (; match < exe_size; ++match)
    *build_dir = build_dir->Append(exe_parts[match]);
  return true;
}

javascript_dialogs::AppModalDialogController* WaitForAppModalDialog() {
  auto* dialog_queue = javascript_dialogs::AppModalDialogQueue::GetInstance();
  if (dialog_queue->HasActiveDialog()) {
    AppModalDialogWaiter::CheckForHangMonitorDisabling(
        dialog_queue->active_dialog());
    return dialog_queue->active_dialog();
  }
  AppModalDialogWaiter waiter;
  return waiter.Wait();
}

#if defined(TOOLKIT_VIEWS)
void WaitForViewVisibility(Browser* browser, ViewID vid, bool visible) {
  views::View* view = views::Widget::GetWidgetForNativeWindow(
                          browser->window()->GetNativeWindow())
                          ->GetContentsView()
                          ->GetViewByID(vid);
  ASSERT_TRUE(view);
  if (view->GetVisible() == visible)
    return;

  base::RunLoop run_loop;
  auto subscription = view->AddVisibleChangedCallback(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(visible, view->GetVisible());
}
#endif

int FindInPage(WebContents* tab,
               const std::u16string& search_string,
               bool forward,
               bool match_case,
               int* ordinal,
               gfx::Rect* selection_rect) {
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(tab);
  find_tab_helper->StartFinding(search_string, forward, match_case,
                                true, /* find_match */
                                true /* run_synchronously_for_testing */);
  FindResultWaiter observer(tab);
  observer.Wait();
  if (ordinal)
    *ordinal = observer.active_match_ordinal();
  if (selection_rect)
    *selection_rect = observer.selection_rect();
  return observer.number_of_matches();
}

void DownloadURL(Browser* browser, const GURL& download_url) {
  content::DownloadManager* download_manager =
      browser->profile()->GetDownloadManager();
  std::unique_ptr<content::DownloadTestObserver> observer(
      new content::DownloadTestObserverTerminal(
          download_manager, 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, download_url));
  observer->WaitForFinished();
}

void WaitForAutocompleteDone(Browser* browser) {
  auto* controller = browser->window()
                         ->GetLocationBar()
                         ->GetOmniboxView()
                         ->controller()
                         ->autocomplete_controller();
  while (!controller->done())
    AutocompleteChangeObserver(browser->profile()).Wait();
}

bool WaitForMinimized(Browser* browser) {
  views::test::PropertyWaiter minimize_waiter(
      base::BindRepeating(&BrowserWindow::IsMinimized,
                          base::Unretained(browser->window())),
      true);
  return minimize_waiter.Wait();
}

bool WaitForMaximized(Browser* browser) {
  views::test::PropertyWaiter maximize_waiter(
      base::BindRepeating(&BrowserWindow::IsMaximized,
                          base::Unretained(browser->window())),
      true);
  return maximize_waiter.Wait();
}

views::AsyncWidgetRequestWaiter CreateAsyncWidgetRequestWaiter(
    Browser& browser) {
  auto* widget = views::Widget::GetWidgetForNativeWindow(
      browser.window()->GetNativeWindow());
  CHECK(widget);
  return views::AsyncWidgetRequestWaiter(*widget);
}

void SetAndWaitForBounds(Browser& browser, const gfx::Rect& bounds) {
  auto waiter = CreateAsyncWidgetRequestWaiter(browser);
  auto* window = browser.window();
  window->SetBounds(bounds);
  waiter.Wait();
}

bool MaximizeAndWaitUntilUIUpdateDone(Browser& browser) {
  auto waiter = ui_test_utils::CreateAsyncWidgetRequestWaiter(browser);
  browser.window()->Maximize();
  waiter.Wait();
  return browser.window()->IsMaximized();
}

FullscreenWaiter::FullscreenWaiter(Browser* browser,
                                   FullscreenWaiter::Expectation expectation)
    : expectation_(std::move(expectation)),
      controller_(browser->exclusive_access_manager()->fullscreen_controller()),
      // Sometimes, the wait is called on a sequeunce, e.g.
      // as a part of interactive_ui_tests's RunTestSequence.
      // To handle that case, we can process pending task posted to the
      // sequence in nested RunLoop.
      run_loop_(base::RunLoop::Type::kNestableTasksAllowed),
      satisfied_(IsSatisfied()) {
  observation_.Observe(controller_);
}

FullscreenWaiter::~FullscreenWaiter() = default;

void FullscreenWaiter::Wait() {
  if (satisfied_) {
    return;
  }
  run_loop_.Run();
}

void FullscreenWaiter::OnFullscreenStateChanged() {
  // Note: In Lacros, when full screen mode changes, FullscreenController
  // triggers WindowFullscreenStateChanged twice for the same change
  // asynchronously. If the test code toggles fullscreen mode on and off, there
  // is a race between the second notification of fullscreen mode on and test
  // code toggle fullscreen mode off. Wait until the fullscreen state changes to
  // the expected mode. See details in crbug.com/1481727.
  if (!IsSatisfied()) {
    return;
  }
  satisfied_ = true;
  if (run_loop_.running()) {
    run_loop_.Quit();
  }
}

bool FullscreenWaiter::IsSatisfied() const {
  if (expectation_.browser_fullscreen.has_value() &&
      expectation_.browser_fullscreen.value() !=
          controller_->IsFullscreenForBrowser()) {
    return false;
  }

  if (expectation_.tab_fullscreen.has_value() &&
      expectation_.tab_fullscreen.value() != controller_->IsTabFullscreen()) {
    return false;
  }

  if (expectation_.display_id.has_value()) {
    // Display ID is valid iff the tab fullscreen mode is active.
    if (!controller_->IsTabFullscreen()) {
      return false;
    }
    if (expectation_.display_id.value() !=
        FullscreenController::GetDisplayId(
            *controller_->exclusive_access_tab())) {
      return false;
    }
  }

  return true;
}

void ToggleFullscreenModeAndWait(Browser* browser) {
  // The waiting condition is following the current implementation.
  // If the mode is either browser/tab fullscreen, it will be existed.
  // Otherwise, entering into browser fullscreen.
  bool current = browser->exclusive_access_manager()->context()->IsFullscreen();
  FullscreenWaiter waiter(browser, current ? FullscreenWaiter::kNoFullscreen
                                           : FullscreenWaiter::Expectation{
                                                 .browser_fullscreen = true});
  chrome::ToggleFullscreenMode(browser);
  waiter.Wait();
}

void WaitUntilBrowserBecomeActive(Browser* browser) {
  CHECK(browser);
  views::Widget* widget =
      BrowserView::GetBrowserViewForBrowser(browser)->GetWidget();
  views::test::WaitForWidgetActive(widget, /*active=*/true);
}

bool IsBrowserActive(Browser* browser) {
  CHECK(browser);
  views::Widget* widget =
      BrowserView::GetBrowserViewForBrowser(browser)->GetWidget();
  return widget->native_widget_active();
}

Browser* OpenNewEmptyWindowAndWaitUntilActivated(
    Profile* profile,
    bool should_trigger_session_restore) {
  BrowserChangeObserver new_browser_observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  chrome::NewEmptyWindow(profile, should_trigger_session_restore);
  Browser* new_browser = new_browser_observer.Wait();
  WaitUntilBrowserBecomeActive(new_browser);
  return new_browser;
}

BrowserSetLastActiveWaiter::BrowserSetLastActiveWaiter(
    Browser* browser,
    bool wait_for_set_last_active_observed)
    : browser_(browser),
      wait_for_set_last_active_observed_(wait_for_set_last_active_observed) {
  BrowserList::AddObserver(this);
  if (chrome::FindLastActive() == browser_ &&
      !wait_for_set_last_active_observed_) {
    satisfied_ = true;
  }
}

BrowserSetLastActiveWaiter::~BrowserSetLastActiveWaiter() {
  BrowserList::RemoveObserver(this);
}

// Runs a loop until |browser_| becomes the last active browser.
void BrowserSetLastActiveWaiter::Wait() {
  if (satisfied_) {
    return;
  }

  run_loop_.Run();
}

// BrowserListObserver:
void BrowserSetLastActiveWaiter::OnBrowserSetLastActive(Browser* browser) {
  if (browser == browser_) {
    satisfied_ = true;
    if (run_loop_.running()) {
      run_loop_.Quit();
    }
  }
}

void WaitForBrowserSetLastActive(Browser* browser,
                                 bool wait_for_set_last_active_observed) {
  BrowserSetLastActiveWaiter waiter(browser, wait_for_set_last_active_observed);
  waiter.Wait();
}

void SendToOmniboxAndSubmit(Browser* browser,
                            const std::string& input,
                            base::TimeTicks match_selection_timestamp) {
  OmniboxView* omnibox = browser->window()->GetLocationBar()->GetOmniboxView();
  omnibox->model()->OnSetFocus(/*control_down=*/false);
  omnibox->SetUserText(base::ASCIIToUTF16(input));
  omnibox->model()->OpenSelection(match_selection_timestamp);

  WaitForAutocompleteDone(browser);
}

Browser* GetBrowserNotInSet(const std::set<Browser*>& excluded_browsers) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (excluded_browsers.find(browser) == excluded_browsers.end())
      return browser;
  }
  return nullptr;
}

namespace {

void GetCookieCallback(base::RepeatingClosure callback,
                       net::CookieList* cookies,
                       const net::CookieAccessResultList& cookie_list,
                       const net::CookieAccessResultList& excluded_cookies) {
  *cookies = net::cookie_util::StripAccessResults(cookie_list);
  callback.Run();
}

}  // namespace

void GetCookies(const GURL& url,
                WebContents* contents,
                int* value_size,
                std::string* value) {
  *value_size = -1;
  if (url.is_valid() && contents) {
    base::RunLoop loop;
    auto* storage_partition =
        contents->GetPrimaryMainFrame()->GetProcess()->GetStoragePartition();
    net::CookieList cookie_list;
    storage_partition->GetCookieManagerForBrowserProcess()->GetCookieList(
        url, net::CookieOptions::MakeAllInclusive(),
        net::CookiePartitionKeyCollection(),
        base::BindOnce(GetCookieCallback, loop.QuitClosure(), &cookie_list));
    loop.Run();

    *value = net::CanonicalCookie::BuildCookieLine(cookie_list);
    *value_size = static_cast<int>(value->size());
  }
}

// It would be nice to `AddAllBrowsers()` here, but we have to wait until our
// subclass is constructed to `ProcessOneBrowser()`.  We can't put it off
// until `Wait()` since we need to watch for anything that happens between now
// and then.
AllTabsObserver::AllTabsObserver() = default;

AllTabsObserver::~AllTabsObserver() {
  BrowserList::RemoveObserver(this);
  // We're tracking all browsers, so remove us from all of all of them.
  for (Browser* browser : *BrowserList::GetInstance()) {
    browser->tab_strip_model()->RemoveObserver(this);
  }
}

void AllTabsObserver::AddAllBrowsers() {
  added_all_browsers_ = true;
  BrowserList::AddObserver(this);
  for (Browser* browser : *BrowserList::GetInstance()) {
    AddBrowser(browser);
  }
}

void AllTabsObserver::Wait() {
  // For subclasses that can detect if their condition is met without
  // necessarily needing to watch for transitions, we could wait until now to
  // call `AddAllBrowsers()` if it hasn't happened yet.
  CHECK(added_all_browsers_)
      << "Subclasses must call `AddAllBrowsers()` during construction";

  if (!condition_met_) {
    run_loop_ = std::make_unique<base::RunLoop>(
        base::RunLoop::Type::kNestableTasksAllowed);
    run_loop_->Run();
    run_loop_.reset();
  }
  EXPECT_TRUE(condition_met_);
}

// impls should call this to tell us to stop waiting.
void AllTabsObserver::ConditionMet() {
  condition_met_ = true;
  if (run_loop_) {
    run_loop_->Quit();
  }
}

void AllTabsObserver::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() != TabStripModelChange::kInserted) {
    return;
  }

  AddWebContents(change.GetInsert()->contents[0].contents.get());
}

void AllTabsObserver::OnBrowserAdded(Browser* browser) {
  AddBrowser(browser);
}

void AllTabsObserver::AddBrowser(const Browser* browser) {
  browser->tab_strip_model()->AddObserver(this);
  // Enumerate all WebContents in this browser, and add a WebContentsObserver
  // for it.
  for (int index = 0; index < browser->tab_strip_model()->count(); index++) {
    auto* web_contents = browser->tab_strip_model()->GetWebContentsAt(index);
    AddWebContents(web_contents);
  }
}

void AllTabsObserver::AddWebContents(content::WebContents* web_contents) {
  // If the condition is already met, then don't bother.
  if (condition_met_) {
    return;
  }

  auto observer = ProcessOneContents(web_contents);
  if (observer) {
    // Store both the subclass's observer and our own with this WebContents.
    // We'll handle cleaning them both up on destruction.  It's okay if our
    // subclass does not create an observer, but does notify us to stop waiting
    // before returning.
    tab_navigation_map_[web_contents].subclass_observer = std::move(observer);
    tab_navigation_map_[web_contents].destruction_observer =
        std::make_unique<WebContentsDestructionObserver>(
            base::BindOnce(&AllTabsObserver::OnWebContentsDestroyed,
                           base::Unretained(this),
                           base::Unretained(web_contents)),
            web_contents);
  }
}

// called by our destruction observers
void AllTabsObserver::OnWebContentsDestroyed(WebContents* web_contents) {
  auto iter = tab_navigation_map_.find(web_contents);
  CHECK(iter != tab_navigation_map_.end());
  // This clears both our observer and the one created by our subclass.
  tab_navigation_map_.erase(iter);
}

AllTabsObserver::TabNavigationMapEntry::TabNavigationMapEntry() = default;
AllTabsObserver::TabNavigationMapEntry::~TabNavigationMapEntry() = default;

UrlLoadObserver::UrlLoadObserver(const GURL& url) : url_(url) {
  AddAllBrowsers();
}

UrlLoadObserver::~UrlLoadObserver() = default;

std::unique_ptr<base::CheckedObserver> UrlLoadObserver::ProcessOneContents(
    WebContents* web_contents) {
  return std::make_unique<LoadStopObserver>(this, web_contents);
}

void UrlLoadObserver::OnDidStopLoading(WebContents* web_contents) {
  NavigationController& controller = web_contents->GetController();
  NavigationEntry* entry = controller.GetVisibleEntry();
  if (!entry || entry->GetVirtualURL() != url_) {
    return;
  }

  // Record the first match.
  if (!web_contents_) {
    web_contents_ = web_contents;
  }
  ConditionMet();
}

UrlLoadObserver::LoadStopObserver::LoadStopObserver(UrlLoadObserver* owner,
                                                    WebContents* web_contents)
    : WebContentsObserver(web_contents), owner_(owner) {}

UrlLoadObserver::LoadStopObserver::~LoadStopObserver() = default;

void UrlLoadObserver::LoadStopObserver::DidStopLoading() {
  owner_->OnDidStopLoading(web_contents());
}

HistoryEnumerator::HistoryEnumerator(Profile* profile) {
  base::RunLoop run_loop;
  base::CancelableTaskTracker tracker;

  HistoryServiceFactory::GetForProfile(profile,
                                       ServiceAccessType::EXPLICIT_ACCESS)
      ->QueryHistory(
          std::u16string(), history::QueryOptions(),
          base::BindLambdaForTesting([&](history::QueryResults results) {
            for (const auto& item : results)
              urls_.push_back(item.url());
            run_loop.Quit();
          }),
          &tracker);
  run_loop.Run();
}

HistoryEnumerator::~HistoryEnumerator() {}

// Wait for HistoryService to load.
class WaitHistoryLoadedObserver : public history::HistoryServiceObserver {
 public:
  explicit WaitHistoryLoadedObserver(content::MessageLoopRunner* runner);
  ~WaitHistoryLoadedObserver() override;

  // history::HistoryServiceObserver:
  void OnHistoryServiceLoaded(history::HistoryService* service) override;

 private:
  // weak
  raw_ptr<content::MessageLoopRunner> runner_;
};

WaitHistoryLoadedObserver::WaitHistoryLoadedObserver(
    content::MessageLoopRunner* runner)
    : runner_(runner) {
}

WaitHistoryLoadedObserver::~WaitHistoryLoadedObserver() {
}

void WaitHistoryLoadedObserver::OnHistoryServiceLoaded(
    history::HistoryService* service) {
  runner_->Quit();
}

void WaitForHistoryToLoad(history::HistoryService* history_service) {
  if (!history_service->BackendLoaded()) {
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    WaitHistoryLoadedObserver observer(runner.get());
    base::ScopedObservation<history::HistoryService,
                            history::HistoryServiceObserver>
        scoped_observer(&observer);
    scoped_observer.Observe(history_service);
    runner->Run();
  }
}

Browser* WaitForBrowserToOpen() {
  return BrowserChangeObserver(nullptr,
                               BrowserChangeObserver::ChangeType::kAdded)
      .Wait();
}

void WaitForBrowserToClose(Browser* browser) {
  BrowserChangeObserver(browser, BrowserChangeObserver::ChangeType::kRemoved)
      .Wait();
}

TabAddedWaiter::TabAddedWaiter(Browser* browser) {
  browser->tab_strip_model()->AddObserver(this);
}

content::WebContents* TabAddedWaiter::Wait() {
  TRACE_EVENT0("test", "TabAddedWaiter::Wait");
  run_loop_.Run();
  return web_contents_;
}

void TabAddedWaiter::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (web_contents_) {
    return;
  }
  if (change.type() == TabStripModelChange::kInserted) {
    web_contents_ = change.GetInsert()->contents[0].contents;
    run_loop_.Quit();
  }
}

AllBrowserTabAddedWaiter::AllBrowserTabAddedWaiter() {
  BrowserList::AddObserver(this);
  for (const Browser* browser : *BrowserList::GetInstance())
    browser->tab_strip_model()->AddObserver(this);
}

AllBrowserTabAddedWaiter::~AllBrowserTabAddedWaiter() {
  BrowserList::RemoveObserver(this);
}

content::WebContents* AllBrowserTabAddedWaiter::Wait() {
  run_loop_.Run();
  return web_contents_;
}

void AllBrowserTabAddedWaiter::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (web_contents_)
    return;

  if (change.type() != TabStripModelChange::kInserted)
    return;

  web_contents_ = change.GetInsert()->contents[0].contents;
  run_loop_.Quit();
}

void AllBrowserTabAddedWaiter::OnBrowserAdded(Browser* browser) {
  browser->tab_strip_model()->AddObserver(this);
}

BrowserChangeObserver::BrowserChangeObserver(Browser* browser, ChangeType type)
    : browser_(browser), type_(type) {
  BrowserList::AddObserver(this);
}

BrowserChangeObserver::~BrowserChangeObserver() {
  BrowserList::RemoveObserver(this);
}

Browser* BrowserChangeObserver::Wait() {
  run_loop_.Run();
  return browser_;
}

void BrowserChangeObserver::OnBrowserAdded(Browser* browser) {
  if (type_ == ChangeType::kAdded) {
    browser_ = browser;
    run_loop_.Quit();
  }
}

void BrowserChangeObserver::OnBrowserRemoved(Browser* browser) {
  if (browser_ && browser_ != browser)
    return;

  if (type_ == ChangeType::kRemoved) {
    browser_ = browser;
    run_loop_.Quit();
  }
}

///////////////////////////////////////////////////////////////////////////////
// CheckWaiter:

CheckWaiter::CheckWaiter(base::RepeatingCallback<bool()> callback,
                         bool expected,
                         const base::TimeDelta& timeout)
    : callback_(callback),
      expected_(expected),
      timeout_(base::TimeTicks::Now() + timeout) {}

CheckWaiter::~CheckWaiter() = default;

void CheckWaiter::Wait() {
  if (Check()) {
    return;
  }

  base::RunLoop run_loop;
  quit_ = run_loop.QuitClosure();
  run_loop.Run();
}

bool CheckWaiter::Check() {
  if (callback_.Run() != expected_ && base::TimeTicks::Now() < timeout_) {
    // Check again after a short timeout. Important: Don't use an immediate
    // task to check again, because the pump would be allowed to run it
    // immediately without processing system events (system events are
    // required for the state to change).
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&CheckWaiter::Check),
                       base::Unretained(this)),
        TestTimeouts::tiny_timeout());
    return false;
  }

  // Quit the run_loop to end the wait.
  if (!quit_.is_null()) {
    std::move(quit_).Run();
  }
  return true;
}

ViewBoundsWaiter::ViewBoundsWaiter(views::View* observed_view)
    : observed_view_(observed_view) {
  observed_view_->AddObserver(this);
}

ViewBoundsWaiter::~ViewBoundsWaiter() {
  observed_view_->RemoveObserver(this);
}

void ViewBoundsWaiter::WaitForNonEmptyBounds() {
  if (!observed_view_->bounds().IsEmpty()) {
    return;
  }
  run_loop_.Run();
}

void ViewBoundsWaiter::OnViewBoundsChanged(views::View* observed_view) {
  if (!observed_view_->bounds().IsEmpty()) {
    run_loop_.Quit();
  }
}

}  // namespace ui_test_utils
