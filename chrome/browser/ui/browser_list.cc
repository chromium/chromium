// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_list.h"

#include <algorithm>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/observer_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list_enumerator.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"

using base::UserMetricsAction;

// static
base::LazyInstance<base::ObserverList<BrowserListObserver>,
                   BrowserList::ObserverListTraits>
    BrowserList::observers_ = LAZY_INSTANCE_INITIALIZER;

// static
BrowserList* BrowserList::instance_ = nullptr;

////////////////////////////////////////////////////////////////////////////////
// BrowserList, public:

// static
BrowserList* BrowserList::GetInstance() {
  BrowserList** list = &instance_;
  if (!*list) {
    *list = new BrowserList;
  }
  return *list;
}

// static
void BrowserList::AddBrowser(Browser* browser) {
  DCHECK(browser);
  DCHECK(browser->window()) << "Browser should not be added to BrowserList "
                               "until it is fully constructed.";
  GetInstance()->browsers_.push_back(browser);

  browser->RegisterKeepAlive();

  AddBrowserToActiveList(browser);

  for (BrowserListObserver& observer : observers_.Get()) {
    observer.OnBrowserAdded(browser);
  }

  if (browser->profile()->IsGuestSession()) {
    base::UmaHistogramCounts100("Browser.WindowCount.Guest",
                                chrome::GetGuestBrowserCount());
  } else if (browser->profile()->IsIncognitoProfile()) {
    base::UmaHistogramCounts100(
        "Browser.WindowCount.Incognito",
        chrome::GetOffTheRecordBrowsersActiveForProfile(browser->profile()));
  }
}

// static
void BrowserList::RemoveBrowser(Browser* browser) {
  // Remove |browser| from the appropriate list instance.
  BrowserList* browser_list = GetInstance();
  RemoveBrowserFrom(browser, &browser_list->browsers_ordered_by_activation_);

  RemoveBrowserFrom(browser, &browser_list->browsers_);

  for (BrowserListObserver& observer : observers_.Get()) {
    observer.OnBrowserRemoved(browser);
  }

  browser->UnregisterKeepAlive();

  // If we're exiting, send out the APP_TERMINATING notification to allow other
  // modules to shut themselves down.
  if (!KeepAliveRegistry::GetInstance()->IsOriginRegistered(
          KeepAliveOrigin::BROWSER) &&
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
  if (browser->IsActive()) {
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
void BrowserList::SetLastActive(Browser* browser) {
  BrowserList* instance = GetInstance();
  DCHECK(std::ranges::contains(instance->browsers_, browser))
      << "SetLastActive called for a browser before the browser was added to "
         "the BrowserList.";
  DCHECK(browser->window())
      << "SetLastActive called for a browser with no window set.";

  base::RecordAction(UserMetricsAction("ActiveBrowserChanged"));

  RemoveBrowserFrom(browser, &instance->browsers_ordered_by_activation_);
  instance->browsers_ordered_by_activation_.push_back(browser);

  for (BrowserListObserver& observer : observers_.Get()) {
    observer.OnBrowserSetLastActive(browser);
  }
}

// static
void BrowserList::NotifyBrowserNoLongerActive(Browser* browser) {
  BrowserList* instance = GetInstance();
  DCHECK(std::ranges::contains(instance->browsers_, browser))
      << "NotifyBrowserNoLongerActive called for a browser before the browser "
         "was added to the BrowserList.";
  DCHECK(browser->window())
      << "NotifyBrowserNoLongerActive called for a browser with no window set.";

  for (BrowserListObserver& observer : observers_.Get()) {
    observer.OnBrowserNoLongerActive(browser);
  }
}

////////////////////////////////////////////////////////////////////////////////
// BrowserList, private:

BrowserList::BrowserList() = default;

BrowserList::~BrowserList() = default;

// static
void BrowserList::RemoveBrowserFrom(Browser* browser,
                                    BrowserVector* browser_list) {
  auto remove_browser = std::ranges::find(*browser_list, browser);
  if (remove_browser != browser_list->end()) {
    browser_list->erase(remove_browser);
  }
}
