// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_tab_strip_tracker.h"

#include "base/auto_reset.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

BrowserTabStripTracker::BrowserTabStripTracker(
    TabStripModelObserver* tab_strip_model_observer,
    BrowserTabStripTrackerDelegate* delegate)
    : tab_strip_model_observer_(tab_strip_model_observer),
      delegate_(delegate),
      is_processing_initial_browsers_(false) {
  DCHECK(tab_strip_model_observer_);
}

BrowserTabStripTracker::~BrowserTabStripTracker() {
  BrowserList::RemoveObserver(this);
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [this](BrowserWindowInterface* browser) {
        browser->GetTabStripModel()->RemoveObserver(tab_strip_model_observer_);
        return true;
      });
}

void BrowserTabStripTracker::Init() {
  base::AutoReset<bool> resetter(&is_processing_initial_browsers_, true);
  ForEachCurrentAndNewBrowserWindowInterfaceOrderedByActivation(
      [this](BrowserWindowInterface* browser) {
        MaybeTrackBrowser(browser);
        return true;
      });
  BrowserList::AddObserver(this);
}

bool BrowserTabStripTracker::ShouldTrackBrowser(
    BrowserWindowInterface* browser) {
  return !delegate_ || delegate_->ShouldTrackBrowser(browser);
}

void BrowserTabStripTracker::MaybeTrackBrowser(
    BrowserWindowInterface* browser) {
  if (!ShouldTrackBrowser(browser)) {
    return;
  }

  TabStripModel* const tab_strip_model = browser->GetTabStripModel();
  tab_strip_model->AddObserver(tab_strip_model_observer_);

  TabStripModelChange::Insert insert;
  insert.contents.reserve(tab_strip_model->count());
  for (int i = 0; tabs::TabInterface* tab : *tab_strip_model) {
    insert.contents.push_back({tab, tab->GetContents(), i});
    ++i;
  }

  TabStripModelChange change(std::move(insert));
  TabStripSelectionChange selection(tab_strip_model->GetActiveTab(),
                                    tab_strip_model->selection_model());
  tab_strip_model_observer_->OnTabStripModelChanged(tab_strip_model, change,
                                                    selection);
}

void BrowserTabStripTracker::OnBrowserAdded(Browser* browser) {
  MaybeTrackBrowser(browser);
}

void BrowserTabStripTracker::OnBrowserRemoved(Browser* browser) {
  // Per ObserverList::RemoveObserver() documentation, this does nothing if the
  // observer is not in the ObserverList (i.e. if |browser| is not tracked).
  browser->GetTabStripModel()->RemoveObserver(tab_strip_model_observer_);
}
