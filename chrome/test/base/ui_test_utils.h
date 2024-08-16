// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_UI_TEST_UTILS_H_
#define CHROME_TEST_BASE_UI_TEST_UTILS_H_

#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/view_ids.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/view_observer.h"
#include "url/gurl.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/views/test/widget_test_api.h"
#endif

class Browser;
class FullscreenController;
class Profile;

namespace javascript_dialogs {
class AppModalDialogController;
}

namespace base {
class FilePath;
}

struct NavigateParams;

namespace content {
class RenderFrameHost;
class WebContents;
}

namespace gfx {
class Rect;
}

namespace views {
class View;
}

// A collections of functions designed for use with InProcessBrowserTest.
namespace ui_test_utils {

// Flags to indicate what to wait for in a navigation test.
// They can be ORed together.
// The order in which the waits happen when more than one is selected, is:
//    Browser
//    Tab
//    Navigation
enum BrowserTestWaitFlags {
  // Don't wait for anything.
  BROWSER_TEST_NO_WAIT = 0,
  // Wait for a new browser.
  BROWSER_TEST_WAIT_FOR_BROWSER = 1 << 0,
  // Wait for a new tab.
  BROWSER_TEST_WAIT_FOR_TAB = 1 << 1,
  // Wait for loading to stop. Loading stops when either
  // a document and its subresources are completely loaded
  // (i.e. the spinner has stopped) or no document can be
  // loaded due to an e.g. an error or crash.
  BROWSER_TEST_WAIT_FOR_LOAD_STOP = 1 << 2,

  BROWSER_TEST_MASK = BROWSER_TEST_WAIT_FOR_BROWSER |
                      BROWSER_TEST_WAIT_FOR_TAB |
                      BROWSER_TEST_WAIT_FOR_LOAD_STOP
};

// Puts the current tab title in |title|. Returns true on success.
bool GetCurrentTabTitle(const Browser* browser, std::u16string* title);

// NavigateToURL* functions navigate the given |browser| to |url| according the
// provided parameters and block until ready (by default - until loading stops,
// see BROWSER_TEST_WAIT_FOR_LOAD_STOP for more details. Note that this is
// different from content::NavigateToURL, which block only until navigation
// succeeds or fails, which generally happens earlier).
//
// Some of these functions return RenderFrameHost* where the navigation was
// committed or nullptr if the navigation failed. The caller should inspect the
// return value - typically with: ASSERT_TRUE(NavigateToURL(...)).
//
// Note: if the navigation has committed, this doesn't mean that the old
// RenderFrameHost was destroyed:
// - it either can wait for the renderer process to finish running unload
//   handlers and acknowledge that.
// - it can be stored in BackForwardCache to be reused for subsequent
//   back/forward navigation.
//
// If the test needs to test RenderFrameHost cleanup, use
// BackForwardCache::DisableForTesting to ensure that RenderFrameHost isn't
// preserved in BackForwardCache and
// RenderFrameDeletedObserver::WaitUntilDeleted to wait for deletion.

// Navigate according to |params|.
void NavigateToURL(NavigateParams* params);

// Navigate current tab of the |browser| to |url| using POST request, simulating
// form submission.
void NavigateToURLWithPost(Browser* browser, const GURL& url);

// Navigate current tab of the |browser| to |url|, simulating a user typing
// |url| into the omnibox.
[[nodiscard]] content::RenderFrameHost* NavigateToURL(Browser* browser,
                                                      const GURL& url);

// Same as |NavigateToURL|, but:
// - |disposition| allows to specify in which tab navigation should happen
// - |browser_test_flags| allows to specify a different condition this function
//   would wait until, see BrowserTestWaitFlags for details.
content::RenderFrameHost* NavigateToURLWithDisposition(
    Browser* browser,
    const GURL& url,
    WindowOpenDisposition disposition,
    int browser_test_flags);

// Same as |NavigateToURL|, but wait for a given number of navigations to
// complete instead of the tab to finish loading.
content::RenderFrameHost* NavigateToURLBlockUntilNavigationsComplete(
    Browser* browser,
    const GURL& url,
    int number_of_navigations);

// See |NavigateToURLWithDisposition| and
// |NavigateToURLBlockUntilNavigationsComplete|.
content::RenderFrameHost*
NavigateToURLWithDispositionBlockUntilNavigationsComplete(
    Browser* browser,
    const GURL& url,
    int number_of_navigations,
    WindowOpenDisposition disposition,
    int browser_test_flags);

// Generate the file path for testing a particular test.
// The file for the tests is all located in
// test_root_directory/dir/<file>
// The returned path is base::FilePath format.
base::FilePath GetTestFilePath(const base::FilePath& dir,
                               const base::FilePath& file);

// Generate the URL for testing a particular test.
// HTML for the tests is all located in
// test_root_directory/dir/<file>
// The returned path is GURL format.
GURL GetTestUrl(const base::FilePath& dir, const base::FilePath& file);

// Generate the path of the build directory, relative to the source root.
bool GetRelativeBuildDirectory(base::FilePath* build_dir);

// Blocks until an application modal dialog is shown and returns it.
javascript_dialogs::AppModalDialogController* WaitForAppModalDialog();

#if defined(TOOLKIT_VIEWS)
// Blocks until the given view attains the given visibility state.
void WaitForViewVisibility(Browser* browser, ViewID vid, bool visible);
#endif

// Performs a find in the page of the specified tab. Returns the number of
// matches found.  |ordinal| is an optional parameter which is set to the index
// of the current match. |selection_rect| is an optional parameter which is set
// to the location of the current match.
int FindInPage(content::WebContents* tab,
               const std::u16string& search_string,
               bool forward,
               bool case_sensitive,
               int* ordinal,
               gfx::Rect* selection_rect);

// Blocks until the |history_service|'s history finishes loading.
void WaitForHistoryToLoad(history::HistoryService* history_service);

// Blocks until a Browser is added to the BrowserList.
Browser* WaitForBrowserToOpen();

// Blocks until a Browser is removed from the BrowserList. If |browser| is null,
// the removal of any browser will suffice; otherwise the removed browser must
// match |browser|.
void WaitForBrowserToClose(Browser* browser = nullptr);

// Download the given file and waits for the download to complete.
void DownloadURL(Browser* browser, const GURL& download_url);

// Waits until the autocomplete controller reaches its done state.
void WaitForAutocompleteDone(Browser* browser);

// Waits until the window gets minimized.
// Returns success or not.
bool WaitForMinimized(Browser* browser);

// Waits until the window gets maximized.
// Returns success or not.
bool WaitForMaximized(Browser* browser);

// See comment on views::AsyncWidgetRequestWaiter.
[[nodiscard]] views::AsyncWidgetRequestWaiter CreateAsyncWidgetRequestWaiter(
    Browser& browser);

// SetAndWaitForBounds sets the given `bounds` on `browser` and waits until the
// bounds update will be observable from all parts of the client (on Wayland).
// This does not verify the resulting bounds.
void SetAndWaitForBounds(Browser& browser, const gfx::Rect& bounds);

// Maximizes the browser window and wait until the window is maximized and all
// related visible UI effects are applied and observable from chrome.
// Returns true if succeeded.
bool MaximizeAndWaitUntilUIUpdateDone(Browser& browser);

// Waits for fullscreen state to be updated.
// There're two variation of fullscreen concepts, browser fullscreen and
// tab fullscreen. Due to fullscreen implementation, fullscreen state may
// be updated synchronously, while observer invocations and some other
// following tasks are done asynchronously.
// This class checks the condition on instance creation, then every
// OnFullscreenStateChanged invocation to deal with the situation.
// Once the condition is met, this class remembers the state, so following
// Wait() will do nothing, even if the condition is changed once again.
class FullscreenWaiter : public FullscreenObserver {
 public:
  // The conditions to be satisfied. std::nullopt means to ignore the
  // value.
  struct Expectation {
    // Condition for IsFullscreenForBrowser() to satisfy.
    std::optional<bool> browser_fullscreen;
    // Condition for IsTabFullscreen() to satisfy.
    std::optional<bool> tab_fullscreen;
    // ID of the display to be used for the fullscreen.
    std::optional<int64_t> display_id;
  };
  // Shortcut constant representing no fullscreen is enabled.
  inline static constexpr Expectation kNoFullscreen = {
      .browser_fullscreen = false,
      .tab_fullscreen = false,
  };

  FullscreenWaiter(Browser* browser, Expectation expecation);

  FullscreenWaiter(const FullscreenWaiter&) = delete;
  FullscreenWaiter& operator=(const FullscreenWaiter&) = delete;
  ~FullscreenWaiter() override;

  // Waits for the fullscreen state(s) to be satisfied.
  // Once it is satisfied after creation, this will do nothing,
  // even if the state is changed once again, and does not satisfy
  // the condition on calling Wait().
  void Wait();

  // FullscreenObserver:
  void OnFullscreenStateChanged() override;

 private:
  // Checks whether the condition is satisfied now.
  bool IsSatisfied() const;

  const Expectation expectation_;
  const raw_ptr<FullscreenController> controller_;
  base::ScopedObservation<FullscreenController, FullscreenObserver>
      observation_{this};
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};

  // Caches if the condition is satisfied even once.
  bool satisfied_;
};

// This waiter waits for the specified |browser| becoming the last active
// browser in BrowserList. In Lacros, BrowserList::SetLastActive is triggered by
// OnWidgetActivationChanged when wayland notify the UI change asynchronously.
// Many testing code needs to wait until the expected browser to be set as
// the last active browser, and some testing code needs to wait until
// BrowserList::OnSetLastActive() is observed.
class BrowserSetLastActiveWaiter : public BrowserListObserver {
 public:
  // By default, the waiting will be satisfied if the expected |browser| is the
  // last active browser in BrowserList. In most cases, the testing code
  // depending on chrome::FindLastActive() should be good.
  // In some cases, for example, when there is only one browser in the
  // BrowserList, |browser| can be returned as the last active browser even if
  // the asynchronous Wayland UI event has not arrived yet (i.e.
  // BrowserList::SetLastActive() is not triggered and the code observing
  // BrowserList::OnSetLastActive() will not be called). If the test case
  // depends on the code observing BrowserList::OnSetLastActive() being executed
  // first, we can configure the waiter to be satisfied upon
  // OnBrowserSetLastActive is observed by passing
  // |wait_for_set_last_active_observed| being true.
  explicit BrowserSetLastActiveWaiter(
      Browser* browser,
      bool wait_for_set_last_active_observed = false);
  BrowserSetLastActiveWaiter(const BrowserSetLastActiveWaiter&) = delete;
  BrowserSetLastActiveWaiter& operator=(const BrowserSetLastActiveWaiter&) =
      delete;

  ~BrowserSetLastActiveWaiter() override;

  // Runs a loop until |browser_| becomes the last active browser.
  void Wait();

  // BrowserListObserver:
  void OnBrowserSetLastActive(Browser* browser) override;

 private:
  const raw_ptr<Browser> browser_;  // not_owned
  bool satisfied_ = false;
  bool wait_for_set_last_active_observed_ = false;
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
};

// Toggles browser fullscreen mode, then wait for its completion.
void ToggleFullscreenModeAndWait(Browser* browser);

// Waits until |browser| becomes active.
void WaitUntilBrowserBecomeActive(Browser* browser);

// Returns true if |browser| is active.
bool IsBrowserActive(Browser* browser);

// Opens a new browser window with chrome::NewEmptyWindow() and wait until it
// becomes active.
// Returns newly created browser.
Browser* OpenNewEmptyWindowAndWaitUntilActivated(
    Profile* profile,
    bool should_trigger_session_restore = false);

// Waits for |browser| becomes the last active browser.
// By default, the waiting will be satisfied if the expected |browser| is the
// last active browser in BrowserList. In most cases, this is enough for the
// testing code depending on chrome::FindLastActive(). In some cases, for
// example, when there is only one browser in the BrowserList, |browser| can be
// returned as the last active browser even if the asynchronous Wayland UI event
// has not arrived yet (i.e. BrowserList::SetLastActive() is not triggered and
// the code observing BrowserList::OnSetLastActive() will not be called). If the
// test case depends on the code observing BrowserList::OnSetLastActive() being
// executed first, we can configure the waiter to be satisfied upon
// OnBrowserSetLastActive is observed by passing
// |wait_for_set_last_active_observed| being true.
// Note: The last active browser is not necessarily the current active browser.
// A browser could be de-activated and still the last active browser. In many
// tests, BrowserList::GetLastActive() is incorrectly used to verify the
// expected browser being the active browser, see b/345848530.
void WaitForBrowserSetLastActive(
    Browser* browser,
    bool wait_for_set_last_active_observed = false);

// Send the given text to the omnibox and wait until it's updated.
void SendToOmniboxAndSubmit(
    Browser* browser,
    const std::string& input,
    base::TimeTicks match_selection_timestamp = base::TimeTicks());

// Gets the first browser that is not in the specified set.
Browser* GetBrowserNotInSet(const std::set<Browser*>& excluded_browsers);

// Gets the size and value of the cookie string for |url| in the given tab.
// Can be called from any thread.
void GetCookies(const GURL& url,
                content::WebContents* contents,
                int* value_size,
                std::string* value);

// Utility class to watch all existing and added tabs, until some interesting
// thing has happened.  Subclasses get to decide what they consider to be
// interesting.  In practice, usage is like this:
//
// - Subclass `AllTabsObserver`
// - Override `ProcessOneWebContents()` to check for the interesting thing.
// - Optionally return a `WebContentsObserver` that will watch for the
//   interesting thing for this WebContents.
// - Eventually call `ConditionMet()` to indicate that the interesting thing has
//   happened, and no further waiting is needed.
//
// Users of this class just call `Wait()` at most once.
class AllTabsObserver : public TabStripModelObserver,
                        public BrowserListObserver {
 public:
  AllTabsObserver(const AllTabsObserver&) = delete;
  AllTabsObserver& operator=(const AllTabsObserver&) = delete;

  ~AllTabsObserver() override;

  // Waits until whatever interesting thing we're waiting for has happened.
  // Will return immediately if it's already happened.
  void Wait();

 protected:
  AllTabsObserver();

  // Will be called for every tab's WebContents, including ones that exist when
  // this class is constructed and any that are created afterwards until
  // destruction or until `ConditionMet()` is called.
  //
  // This may choose not to return an observer if there's no need to watch this
  // contents.  It may also call `ConditionMet()` before returning, presumably
  // because the new contents already matches whatever condition our subclass is
  // checking for.  In that case, it will presumably not bother to return an
  // observer for the new contents.
  //
  // These will be deleted before `this` is deleted, so it's okay to have the
  // observers hold raw_ptrs back to `this`.
  virtual std::unique_ptr<base::CheckedObserver> ProcessOneContents(
      content::WebContents* web_contents) = 0;

  // Add all tabs from all browsers.  Must be called by the subclass ctor.
  void AddAllBrowsers();

  // Called by our subclass to let us know that whatever it's trying to wait for
  // has happened.  May be called at any time, including during a call to
  // `CreateObserverIfNeeded()`.  May be called more than once, though
  // calls will be ignored.
  void ConditionMet();

 private:
  // Record for every tab we're watching.
  struct TabNavigationMapEntry {
    TabNavigationMapEntry();
    ~TabNavigationMapEntry();

    // Provided by the subclass to do whatever it does.
    std::unique_ptr<base::CheckedObserver> subclass_observer;
    // Provided by us to clean up properly.
    std::unique_ptr<base::CheckedObserver> destruction_observer;
  };
  using TabNavigationMap =
      std::map<const content::WebContents*, TabNavigationMapEntry>;

  // Add all tabs from `browser`, and start watching for changes.
  void AddBrowser(const Browser* browser);

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // BrowserListObserver
  void OnBrowserAdded(Browser* browser) override;

  // Called for every WebContents.  Notifies the subclass, and sets up observers
  // if needed.
  void AddWebContents(content::WebContents* web_contents);

  // Called by our destruction observers.
  void OnWebContentsDestroyed(content::WebContents* web_contents);

  // Map of how many times each tab has navigated since |this| was created.
  TabNavigationMap tab_navigation_map_;

  // True if WaitForNavigations has been called, until
  // |num_navigations_to_wait_for_| have been observed.
  bool condition_met_ = false;

  // Flag to make sure that subclasses call `AddAllBrowsers()`.
  bool added_all_browsers_ = false;

  std::unique_ptr<base::RunLoop> run_loop_;
};

// Observer which waits for navigation events and blocks until a specific URL is
// loaded. The URL must be an exact match.
class UrlLoadObserver : public AllTabsObserver {
 public:
  // `url` is the URL to look for.
  explicit UrlLoadObserver(const GURL& url);
  ~UrlLoadObserver() override;

  // Returns the WebContents which navigated to `url`.
  content::WebContents* web_contents() const { return web_contents_; }

 protected:
  // Helper class to watch for DidStopLoading on one WebContents and relay it to
  // the UrlLoadObserver that created us.
  class LoadStopObserver : public content::WebContentsObserver {
   public:
    LoadStopObserver(UrlLoadObserver* owner,
                     content::WebContents* web_contents);
    ~LoadStopObserver() override;

    // WebContentsObserver
    void DidStopLoading() override;

   private:
    raw_ptr<UrlLoadObserver> owner_ = nullptr;
  };

  // AllTabsObserver
  std::unique_ptr<base::CheckedObserver> ProcessOneContents(
      content::WebContents* web_contents) override;

  // Called by `LoadStopObserver` when a WebContents DidStopLoading().
  void OnDidStopLoading(content::WebContents* web_contents);

 private:
  friend class LoadStopObserver;

  GURL url_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
};

// A helper that will wait until a tab is added to a specific Browser.
class TabAddedWaiter : public TabStripModelObserver {
 public:
  explicit TabAddedWaiter(Browser* browser);
  TabAddedWaiter(const TabAddedWaiter&) = delete;
  TabAddedWaiter& operator=(const TabAddedWaiter&) = delete;
  ~TabAddedWaiter() override = default;

  content::WebContents* Wait();

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged> web_contents_ =
      nullptr;
};

// Similar to TabAddedWaiter, but will observe tabs added to all Browser
// objects, and can return the last tab that was added.
class AllBrowserTabAddedWaiter : public TabStripModelObserver,
                                 public BrowserListObserver {
 public:
  AllBrowserTabAddedWaiter();
  AllBrowserTabAddedWaiter(const AllBrowserTabAddedWaiter&) = delete;
  AllBrowserTabAddedWaiter& operator=(const AllBrowserTabAddedWaiter&) = delete;
  ~AllBrowserTabAddedWaiter() override;

  content::WebContents* Wait();

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;

 private:
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};

  // The last tab that was added.
  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged> web_contents_ =
      nullptr;
};

// Enumerates all history contents on the backend thread. Returns them in
// descending order by time.
class HistoryEnumerator {
 public:
  explicit HistoryEnumerator(Profile* profile);
  HistoryEnumerator(const HistoryEnumerator&) = delete;
  HistoryEnumerator& operator=(const HistoryEnumerator&) = delete;
  ~HistoryEnumerator();

  std::vector<GURL>& urls() { return urls_; }

 private:
  std::vector<GURL> urls_;
};

// In general, tests should use WaitForBrowserToClose() and
// WaitForBrowserToOpen() rather than instantiating this class directly.
class BrowserChangeObserver : public BrowserListObserver {
 public:
  enum class ChangeType {
    kAdded,
    kRemoved,
  };

  BrowserChangeObserver(Browser* browser, ChangeType type);
  BrowserChangeObserver(const BrowserChangeObserver&) = delete;
  BrowserChangeObserver& operator=(const BrowserChangeObserver&) = delete;
  ~BrowserChangeObserver() override;

  Browser* Wait();

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;

  void OnBrowserRemoved(Browser* browser) override;

 private:
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> browser_;
  ChangeType type_;
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
};

// Encapsulates waiting for the browser window to change state. This is
// needed for example on Chrome desktop linux, where window state change is done
// asynchronously as an event received from a different process.
class CheckWaiter {
 public:
  CheckWaiter(base::RepeatingCallback<bool()> callback,
              bool expected,
              const base::TimeDelta& timeout);
  CheckWaiter(const CheckWaiter&) = delete;
  CheckWaiter& operator=(const CheckWaiter&) = delete;
  ~CheckWaiter();

  // Blocks until the browser window becomes maximized.
  void Wait();

 private:
  bool Check();

  base::RepeatingCallback<bool()> callback_;
  bool expected_;
  const base::TimeTicks timeout_;
  // The waiter's RunLoop quit closure.
  base::RepeatingClosure quit_;
};

// Used to wait for the view to contain non-empty bounds.
class ViewBoundsWaiter : public views::ViewObserver {
 public:
  explicit ViewBoundsWaiter(views::View* observed_view);
  ViewBoundsWaiter(const ViewBoundsWaiter&) = delete;
  ViewBoundsWaiter& operator=(const ViewBoundsWaiter&) = delete;
  ~ViewBoundsWaiter() override;

  // Blocks until the view has non-empty bounds.
  void WaitForNonEmptyBounds();

 private:
  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;

  const raw_ptr<views::View> observed_view_;
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
};

}  // namespace ui_test_utils

#endif  // CHROME_TEST_BASE_UI_TEST_UTILS_H_
