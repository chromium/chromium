// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/pinned_tab_service.h"

#include "base/functional/bind.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/tabs/pinned_tab_codec.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

PinnedTabService::PinnedTabService(Profile* profile) : profile_(profile) {
  closing_all_browsers_subscription_ = chrome::AddClosingAllBrowsersCallback(
      base::BindRepeating(&PinnedTabService::OnClosingAllBrowsersChanged,
                          base::Unretained(this)));

  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [this](BrowserWindowInterface* browser) {
        OnBrowserCreated(browser);
        return true;
      });

  browser_collection_observation_.Observe(
      GlobalBrowserCollection::GetInstance());
}

PinnedTabService::~PinnedTabService() = default;

void PinnedTabService::OnClosingAllBrowsersChanged(bool closing) {
  // Saving of tabs happens when the user exits the application or closes the
  // last browser window. After saving, |need_to_write_pinned_tabs_| is set to
  // false to make sure subsequent window closures don't overwrite the pinned
  // tab state. Saving is re-enabled when a browser window or tab is opened
  // again. Note, cancelling a shutdown (via onbeforeunload) will not re-enable
  // pinned tab saving immediately, to prevent the following situation:
  //   * two windows are open, one with pinned tabs
  //   * user exits
  //   * pinned tabs are saved
  //   * window with pinned tabs is closed
  //   * other window blocks close with onbeforeunload
  //   * user saves work, etc. then closes the window
  //   * pinned tabs are saved, without the window with the pinned tabs,
  //     over-writing the correct state.
  // Saving is re-enabled if a new tab or window is opened.
  if (closing && TabStripModelObserver::IsObservingAny(this)) {
    WritePinnedTabsIfNecessary();
  }
}

void PinnedTabService::OnBrowserCreated(BrowserWindowInterface* browser) {
  if (browser->GetProfile() != profile_ ||
      browser->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
    return;
  }

  need_to_write_pinned_tabs_ = true;
  // TODO(crbug.com/452120900): TabStripModel auto-unregistered by dtor
  browser->GetTabStripModel()->AddObserver(this);
}

void PinnedTabService::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kInserted) {
    need_to_write_pinned_tabs_ = true;
  }
}

void PinnedTabService::WillCloseAllTabs(TabStripModel* tab_strip_model) {
  if (TabStripModelObserver::CountObservedModels(this) == 1) {
    WritePinnedTabsIfNecessary();
  }
}

void PinnedTabService::WritePinnedTabsIfNecessary() {
  if (need_to_write_pinned_tabs_) {
    PinnedTabCodec::WritePinnedTabs(profile_);
  }
  need_to_write_pinned_tabs_ = false;
}
