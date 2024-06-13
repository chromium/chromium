// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_list.h"

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/app_session_service_factory.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"

using base::UserMetricsAction;
using content::WebContents;

namespace {

BrowserList::BrowserWeakVector GetBrowsersToClose(Profile* profile) {
  BrowserList::BrowserWeakVector browsers_to_close;
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile()->GetOriginalProfile() ==
        profile->GetOriginalProfile()) {
      browsers_to_close.push_back(browser->AsWeakPtr());
    }
  }
  return browsers_to_close;
}

BrowserList::BrowserWeakVector GetIncognitoBrowsersToClose(Profile* profile) {
  BrowserList::BrowserWeakVector browsers_to_close;
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile() == profile) {
      browsers_to_close.push_back(browser->AsWeakPtr());
    }
  }
  return browsers_to_close;
}

}  // namespace

// static
base::LazyInstance<base::ObserverList<BrowserListObserver>,
                   BrowserList::ObserverListTraits>
    BrowserList::observers_ = LAZY_INSTANCE_INITIALIZER;

// static
BrowserList* BrowserList::instance_ = nullptr;

////////////////////////////////////////////////////////////////////////////////
// BrowserList, public:

Browser* BrowserList::GetLastActive() const {
  if (!browsers_ordered_by_activation_.empty())
    return *(browsers_ordered_by_activation_.rbegin());
  return nullptr;
}

// static
BrowserList* BrowserList::GetInstance() {
  BrowserList** list = &instance_;
  if (!*list)
    *list = new BrowserList;
  return *list;
}

// static
void BrowserList::AddBrowser(Browser* browser) {
  DCHECK(browser);
  DCHECK(browser->window()) << "Browser should not be added to BrowserList "
                               "until it is fully constructed.";
  GetInstance()->browsers_.push_back(browser);

  browser->RegisterKeepAlive();

  for (BrowserListObserver& observer : observers_.Get())
    observer.OnBrowserAdded(browser);

  AddBrowserToActiveList(browser);

  if (browser->profile()->IsGuestSession()) {
    base::UmaHistogramCounts100("Browser.WindowCount.Guest",
                                GetGuestBrowserCount());
  } else if (browser->profile()->IsIncognitoProfile()) {
    base::UmaHistogramCounts100(
        "Browser.WindowCount.Incognito",
        GetOffTheRecordBrowsersActiveForProfile(browser->profile()));
  }
}

// static
void BrowserList::RemoveBrowser(Browser* browser) {
  // Remove |browser| from the appropriate list instance.
  BrowserList* browser_list = GetInstance();
  RemoveBrowserFrom(browser, &browser_list->browsers_ordered_by_activation_);
  browser_list->currently_closing_browsers_.erase(browser);

  RemoveBrowserFrom(browser, &browser_list->browsers_);

  for (BrowserListObserver& observer : observers_.Get())
    observer.OnBrowserRemoved(browser);

  browser->UnregisterKeepAlive();

  // If we're exiting, send out the APP_TERMINATING notification to allow other
  // modules to shut themselves down.
  if (chrome::GetTotalBrowserCount() == 0 &&
      (browser_shutdown::IsTryingToQuit() ||
       g_browser_process->IsShuttingDown())) {
    // Last browser has just closed, and this is a user-initiated quit or there
    // is no module keeping the app alive, so send out our notification. No need
    // to call ProfileManager::ShutdownSessionServices() as part of the
    // shutdown, because Browser::WindowClosing() already makes sure that the
    // SessionService is created and notified.
    browser_shutdown::NotifyAppTerminating();
    chrome::OnAppExiting();
  }
}

// static
void BrowserList::AddBrowserToActiveList(Browser* browser) {
  if (browser->window()->IsActive()) {
    SetLastActive(browser);
    return;
  }

  // |BrowserList::browsers_ordered_by_activation_| should contain every
  // browser, so prepend any inactive browsers to it.
  BrowserVector* active_browsers =
      &GetInstance()->browsers_ordered_by_activation_;
  RemoveBrowserFrom(browser, active_browsers);
  active_browsers->insert(active_browsers->begin(), browser);
}

// static
void BrowserList::AddObserver(BrowserListObserver* observer) {
  observers_.Get().AddObserver(observer);
}

// static
void BrowserList::RemoveObserver(BrowserListObserver* observer) {
  observers_.Get().RemoveObserver(observer);
}

// static
void BrowserList::CloseAllBrowsersWithProfile(Profile* profile) {
  BrowserVector browsers_to_close;
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile()->GetOriginalProfile() ==
        profile->GetOriginalProfile())
      browsers_to_close.push_back(browser);
  }

  for (BrowserVector::const_iterator it = browsers_to_close.begin();
       it != browsers_to_close.end(); ++it) {
    (*it)->window()->Close();
  }
}

// static
void BrowserList::CloseAllBrowsersWithProfile(
    Profile* profile,
    const CloseCallback& on_close_success,
    const CloseCallback& on_close_aborted,
    bool skip_beforeunload) {
  SessionServiceFactory::ShutdownForProfile(profile);
  AppSessionServiceFactory::ShutdownForProfile(profile);

  TryToCloseBrowserList(GetBrowsersToClose(profile), on_close_success,
                        on_close_aborted, profile->GetPath(),
                        skip_beforeunload);
}

// static
void BrowserList::CloseAllBrowsersWithIncognitoProfile(
    Profile* profile,
    const CloseCallback& on_close_success,
    const CloseCallback& on_close_aborted,
    bool skip_beforeunload) {
  DCHECK(profile->IsOffTheRecord());
  auto browsers_to_close = GetIncognitoBrowsersToClose(profile);

  // When closing devtools browser related to incognito browser, do not skip
  // calling before unload handlers.
  skip_beforeunload =
      skip_beforeunload &&
      base::ranges::none_of(browsers_to_close, &Browser::is_type_devtools);
  TryToCloseBrowserList(browsers_to_close, on_close_success, on_close_aborted,
                        profile->GetPath(), skip_beforeunload);
}

// static
void BrowserList::TryToCloseBrowserList(
    const BrowserWeakVector& browsers_to_close,
    const CloseCallback& on_close_success,
    const CloseCallback& on_close_aborted,
    const base::FilePath& profile_path,
    const bool skip_beforeunload) {
  for (auto& weak_browser : browsers_to_close) {
    if (weak_browser &&
        weak_browser->TryToCloseWindow(
            skip_beforeunload,
            base::BindRepeating(&BrowserList::PostTryToCloseBrowserWindow,
                                browsers_to_close, on_close_success,
                                on_close_aborted, profile_path,
                                skip_beforeunload))) {
      return;
    }
  }

  if (on_close_success)
    on_close_success.Run(profile_path);

  for (auto& weak_b : browsers_to_close) {
    // BeforeUnload handlers may close browser windows, so we need to explicitly
    // check whether they still exist.
    if (weak_b && weak_b->window()) {
      weak_b->window()->Close();
    }
  }
}

// static
void BrowserList::PostTryToCloseBrowserWindow(
    const BrowserWeakVector& browsers_to_close,
    const CloseCallback& on_close_success,
    const CloseCallback& on_close_aborted,
    const base::FilePath& profile_path,
    const bool skip_beforeunload,
    bool tab_close_confirmed) {
  // We need this bool to avoid infinite recursion when resetting the
  // BeforeUnload handlers, since doing that will trigger calls back to this
  // method for each affected window.
  static bool resetting_handlers = false;

  if (tab_close_confirmed) {
    TryToCloseBrowserList(browsers_to_close, on_close_success, on_close_aborted,
                          profile_path, skip_beforeunload);
  } else if (!resetting_handlers) {
    base::AutoReset<bool> resetting_handlers_scoper(&resetting_handlers, true);
    for (auto& weak_browser : browsers_to_close) {
      // This function is called asynchronously, so that the Browser may have
      // been destroyed by the time we get here.
      if (weak_browser) {
        weak_browser->ResetTryToCloseWindow();
      }
    }
    if (on_close_aborted)
      on_close_aborted.Run(profile_path);
  }
}

// static
void BrowserList::MoveBrowsersInWorkspaceToFront(
    const std::string& new_workspace) {
  DCHECK(!new_workspace.empty());

  BrowserList* instance = GetInstance();

  Browser* old_last_active = instance->GetLastActive();
  BrowserVector& last_active_browsers =
      instance->browsers_ordered_by_activation_;

  // Perform a stable partition on the browsers in the list so that the browsers
  // in the new workspace appear after the browsers in the other workspaces.
  //
  // For example, if we have a list of browser-workspace pairs
  // [{b1, 0}, {b2, 1}, {b3, 0}, {b4, 1}]
  // and we switch to workspace 1, we want the resulting browser list to look
  // like [{b1, 0}, {b3, 0}, {b2, 1}, {b4, 1}].
  std::stable_partition(
      last_active_browsers.begin(), last_active_browsers.end(),
      [&new_workspace](Browser* browser) {
        return !browser->window()->IsVisibleOnAllWorkspaces() &&
               browser->window()->GetWorkspace() != new_workspace;
      });

  Browser* new_last_active = instance->GetLastActive();
  if (old_last_active != new_last_active) {
    for (BrowserListObserver& observer : observers_.Get())
      observer.OnBrowserSetLastActive(new_last_active);
  }
}

// static
void BrowserList::SetLastActive(Browser* browser) {
  BrowserList* instance = GetInstance();
  DCHECK(base::Contains(*instance, browser))
      << "SetLastActive called for a browser before the browser was added to "
         "the BrowserList.";
  DCHECK(browser->window())
      << "SetLastActive called for a browser with no window set.";

  base::RecordAction(UserMetricsAction("ActiveBrowserChanged"));

  RemoveBrowserFrom(browser, &instance->browsers_ordered_by_activation_);
  instance->browsers_ordered_by_activation_.push_back(browser);

  for (BrowserListObserver& observer : observers_.Get())
    observer.OnBrowserSetLastActive(browser);
}

// static
void BrowserList::NotifyBrowserNoLongerActive(Browser* browser) {
  BrowserList* instance = GetInstance();
  DCHECK(base::Contains(*instance, browser))
      << "NotifyBrowserNoLongerActive called for a browser before the browser "
         "was added to the BrowserList.";
  DCHECK(browser->window())
      << "NotifyBrowserNoLongerActive called for a browser with no window set.";

  for (BrowserListObserver& observer : observers_.Get())
    observer.OnBrowserNoLongerActive(browser);
}

// static
void BrowserList::NotifyBrowserCloseCancelled(Browser* browser,
                                              BrowserClosingStatus reason) {
  for (BrowserListObserver& observer : observers_.Get()) {
    observer.OnBrowserCloseCancelled(browser, reason);
  }
}

// static
void BrowserList::NotifyBrowserCloseStarted(Browser* browser) {
  GetInstance()->currently_closing_browsers_.insert(browser);

  for (BrowserListObserver& observer : observers_.Get())
    observer.OnBrowserClosing(browser);
}

// static
bool BrowserList::IsOffTheRecordBrowserActive() {
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile()->IsOffTheRecord())
      return true;
  }
  return false;
}

// static
int BrowserList::GetOffTheRecordBrowsersActiveForProfile(Profile* profile) {
  BrowserList* list = BrowserList::GetInstance();
  return base::ranges::count_if(*list, [profile](Browser* browser) {
    return browser->profile()->IsSameOrParent(profile) &&
           browser->profile()->IsOffTheRecord() && !browser->is_type_devtools();
  });
}

// static
size_t BrowserList::GetIncognitoBrowserCount() {
  BrowserList* list = BrowserList::GetInstance();
  return base::ranges::count_if(*list, [](Browser* browser) {
    return browser->profile()->IsIncognitoProfile() &&
           !browser->is_type_devtools();
  });
}

// static
size_t BrowserList::GetGuestBrowserCount() {
  BrowserList* list = BrowserList::GetInstance();
  return base::ranges::count_if(*list, [](Browser* browser) {
    return browser->profile()->IsGuestSession() && !browser->is_type_devtools();
  });
}

// static
bool BrowserList::IsOffTheRecordBrowserInUse(Profile* profile) {
  BrowserList* list = BrowserList::GetInstance();
  return base::ranges::any_of(*list, [profile](Browser* browser) {
    return browser->profile()->IsSameOrParent(profile) &&
           browser->profile()->IsOffTheRecord();
  });
}

////////////////////////////////////////////////////////////////////////////////
// BrowserList, private:

BrowserList::BrowserList() {}

BrowserList::~BrowserList() {}

// static
void BrowserList::RemoveBrowserFrom(Browser* browser,
                                    BrowserVector* browser_list) {
  auto remove_browser = base::ranges::find(*browser_list, browser);
  if (remove_browser != browser_list->end())
    browser_list->erase(remove_browser);
}
