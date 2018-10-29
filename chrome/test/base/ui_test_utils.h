// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_UI_TEST_UTILS_H_
#define CHROME_TEST_BASE_UI_TEST_UTILS_H_

#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
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
class LocationBar;
class Profile;

namespace app_modal {
class JavaScriptAppModalDialog;
}

namespace base {
class FilePath;
}

struct NavigateParams;

namespace content {
class RenderProcessHost;
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
  BROWSER_TEST_NONE = 0,                      // Don't wait for anything.
  BROWSER_TEST_WAIT_FOR_BROWSER = 1 << 0,     // Wait for a new browser.
  BROWSER_TEST_WAIT_FOR_TAB = 1 << 1,         // Wait for a new tab.
  BROWSER_TEST_WAIT_FOR_NAVIGATION = 1 << 2,  // Wait for navigation to finish.

  BROWSER_TEST_MASK = BROWSER_TEST_WAIT_FOR_BROWSER |
                      BROWSER_TEST_WAIT_FOR_TAB |
                      BROWSER_TEST_WAIT_FOR_NAVIGATION
};

// Puts the current tab title in |title|. Returns true on success.
bool GetCurrentTabTitle(const Browser* browser, base::string16* title);

// Performs the provided navigation process, blocking until the navigation
// finishes. May change the params in some cases (i.e. if the navigation
// opens a new browser window). Uses chrome::Navigate.
//
// Note this does not return a RenderProcessHost for where the navigation
// occurs, so tests using this will be unable to verify the destruction of
// the RenderProcessHost when navigating again.
void NavigateToURL(NavigateParams* params);

// Navigates the selected tab of |browser| to |url|, blocking until the
// navigation finishes. Simulates a POST and uses chrome::Navigate.
//
// Note this does not return a RenderProcessHost for where the navigation
// occurs, so tests using this will be unable to verify the destruction of
// the RenderProcessHost when navigating again.
void NavigateToURLWithPost(Browser* browser, const GURL& url);

// Navigates the selected tab of |browser| to |url|, blocking until the
// navigation finishes. Uses Browser::OpenURL --> chrome::Navigate.
//
// Returns a RenderProcessHost* for the renderer where the navigation
// occured. Use this when navigating again, when the test wants to wait not
// just for the navigation to complete but also for the previous
// RenderProcessHost to be torn down. Navigation does NOT imply the old
// RenderProcessHost is gone, and assuming so creates a race condition that
// can be exagerated by artifically slowing the FrameHostMsg_SwapOut_ACK reply
// from the renderer being navigated from.
content::RenderProcessHost* NavigateToURL(Browser* browser, const GURL& url);

// Navigates the specified tab of |browser| to |url|, blocking until the
// navigation finishes.
// |disposition| indicates what tab the navigation occurs in, and
// |browser_test_flags| controls what to wait for before continuing.
//
// If the |browser_test_flags| includes a request to wait for navigation, this
// returns a RenderProcessHost* for the renderer where the navigation
// occured. Use this when navigating again, when the test wants to wait not
// just for the navigation to complete but also for the previous
// RenderProcessHost to be torn down. Navigation does NOT imply the old
// RenderProcessHost is gone, and assuming so creates a race condition that
// can be exagerated by artifically slowing the FrameHostMsg_SwapOut_ACK reply
// from the renderer being navigated from.
content::RenderProcessHost* NavigateToURLWithDisposition(
    Browser* browser,
    const GURL& url,
    WindowOpenDisposition disposition,
    int browser_test_flags);

// Navigates the selected tab of |browser| to |url|, blocking until the
// number of navigations specified complete.
//
// Returns a RenderProcessHost* for the renderer where the navigation
// occured. Use this when navigating again, when the test wants to wait not
// just for the navigation to complete but also for the previous
// RenderProcessHost to be torn down. Navigation does NOT imply the old
// RenderProcessHost is gone, and assuming so creates a race condition that
// can be exagerated by artifically slowing the FrameHostMsg_SwapOut_ACK reply
// from the renderer being navigated from.
content::RenderProcessHost* NavigateToURLBlockUntilNavigationsComplete(
    Browser* browser,
    const GURL& url,
    int number_of_navigations);

// Navigates the specified tab (via |disposition|) of |browser| to |url|,
// blocking until the |number_of_navigations| specified complete.
// |disposition| indicates what tab the download occurs in, and
// |browser_test_flags| controls what to wait for before continuing.
content::RenderProcessHost*
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
app_modal::JavaScriptAppModalDialog* WaitForAppModalDialog();

// Performs a find in the page of the specified tab. Returns the number of
// matches found.  |ordinal| is an optional parameter which is set to the index
// of the current match. |selection_rect| is an optional parameter which is set
// to the location of the current match.
int FindInPage(content::WebContents* tab,
               const base::string16& search_string,
               bool forward,
               bool case_sensitive,
               int* ordinal,
               gfx::Rect* selection_rect);

// Blocks until the |history_service|'s history finishes loading.
void WaitForHistoryToLoad(history::HistoryService* history_service);

// Download the given file and waits for the download to complete.
void DownloadURL(Browser* browser, const GURL& download_url);

// Send the given text to the omnibox and wait until it's updated.
void SendToOmniboxAndSubmit(
    LocationBar* location_bar,
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

// A WindowedNotificationObserver hard-wired to observe
// chrome::NOTIFICATION_TAB_ADDED.
class WindowedTabAddedNotificationObserver
    : public content::WindowedNotificationObserver {
 public:
  // Register to listen for notifications of NOTIFICATION_TAB_ADDED from either
  // a specific source, or from all sources if |source| is
  // NotificationService::AllSources().
  explicit WindowedTabAddedNotificationObserver(
      const content::NotificationSource& source);

  // Returns the added tab, or NULL if no notification was observed yet.
  content::WebContents* GetTab() { return added_tab_; }

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  content::WebContents* added_tab_;

  DISALLOW_COPY_AND_ASSIGN(WindowedTabAddedNotificationObserver);
};

// Similar to WindowedNotificationObserver but also provides a way of retrieving
// the details associated with the notification.
// Note that in order to use that class the details class should be copiable,
// which is the case with most notifications.
template <class U>
class WindowedNotificationObserverWithDetails
    : public content::WindowedNotificationObserver {
 public:
  WindowedNotificationObserverWithDetails(
      int notification_type,
      const content::NotificationSource& source)
      : content::WindowedNotificationObserver(notification_type, source) {}

  // Fills |details| with the details of the notification received for |source|.
  bool GetDetailsFor(uintptr_t source, U* details) {
    typename std::map<uintptr_t, U>::const_iterator iter =
        details_.find(source);
    if (iter == details_.end())
      return false;
    *details = iter->second;
    return true;
  }

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    const U* details_ptr = content::Details<U>(details).ptr();
    if (details_ptr)
      details_[source.map_key()] = *details_ptr;
    content::WindowedNotificationObserver::Observe(type, source, details);
  }

 private:
  std::map<uintptr_t, U> details_;

  DISALLOW_COPY_AND_ASSIGN(WindowedNotificationObserverWithDetails);
};

// Notification observer which waits for navigation events and blocks until
// a specific URL is loaded. The URL must be an exact match.
class UrlLoadObserver : public content::WindowedNotificationObserver {
 public:
  // Register to listen for notifications of the given type from either a
  // specific source, or from all sources if |source| is
  // NotificationService::AllSources().
  UrlLoadObserver(const GURL& url, const content::NotificationSource& source);
  ~UrlLoadObserver() override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(UrlLoadObserver);
};

// Convenience class for waiting for a new browser to be created.
// Like WindowedNotificationObserver, this class provides a safe, non-racey
// way to wait for a new browser to be created.
class BrowserAddedObserver {
 public:
  BrowserAddedObserver();
  ~BrowserAddedObserver();

  // Wait for a new browser to be created, and return a pointer to it.
  Browser* WaitForSingleNewBrowser();

 private:
  content::WindowedNotificationObserver notification_observer_;
  std::set<Browser*> original_browsers_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAddedObserver);
};

// Enumerates all history contents on the backend thread. Returns them in
// descending order by time.
class HistoryEnumerator {
 public:
  explicit HistoryEnumerator(Profile* profile);
  ~HistoryEnumerator();

  std::vector<GURL>& urls() { return urls_; }

 private:
  void HistoryQueryComplete(
      const base::Closure& quit_task,
      history::QueryResults* results);

  std::vector<GURL> urls_;

  base::CancelableTaskTracker tracker_;

  DISALLOW_COPY_AND_ASSIGN(HistoryEnumerator);
};

}  // namespace ui_test_utils

#endif  // CHROME_TEST_BASE_UI_TEST_UTILS_H_
