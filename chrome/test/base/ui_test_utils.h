// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_UI_TEST_UTILS_H_
#define CHROME_TEST_BASE_UI_TEST_UTILS_H_

#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/view_ids.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

class Browser;
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

// Notification observer which waits for navigation events and blocks until
// a specific URL is loaded. The URL must be an exact match.
class UrlLoadObserver : public content::WindowedNotificationObserver {
 public:
  // Register to listen for notifications of the given type from either a
  // specific source, or from all sources if |source| is
  // NotificationService::AllSources().
  UrlLoadObserver(const GURL& url, const content::NotificationSource& source);
  UrlLoadObserver(const UrlLoadObserver&) = delete;
  UrlLoadObserver& operator=(const UrlLoadObserver&) = delete;
  ~UrlLoadObserver() override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  GURL url_;
};

// A helper that will wait until a tab is added to a specific Browser.
class TabAddedWaiter : public TabStripModelObserver {
 public:
  explicit TabAddedWaiter(Browser* browser);
  TabAddedWaiter(const TabAddedWaiter&) = delete;
  TabAddedWaiter& operator=(const TabAddedWaiter&) = delete;
  ~TabAddedWaiter() override = default;

  void Wait();

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  base::RunLoop run_loop_;
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
  base::RunLoop run_loop_;

  // The last tab that was added.
  raw_ptr<content::WebContents, DanglingUntriaged> web_contents_ = nullptr;
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
  raw_ptr<Browser, DanglingUntriaged> browser_;
  ChangeType type_;
  base::RunLoop run_loop_;
};

}  // namespace ui_test_utils

#endif  // CHROME_TEST_BASE_UI_TEST_UTILS_H_
