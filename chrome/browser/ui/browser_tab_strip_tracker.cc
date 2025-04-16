// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_tab_strip_tracker.h"

#include "base/auto_reset.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
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
  BrowserList::GetInstance()->ForEachCurrentBrowser([this](Browser* browser) {
    browser->tab_strip_model()->RemoveObserver(tab_strip_model_observer_);
  });
}

void BrowserTabStripTracker::Init() {
  base::AutoReset<bool> resetter(&is_processing_initial_browsers_, true);
  BrowserList::GetInstance()->ForEachCurrentAndNewBrowser(
      [this](Browser* browser) { MaybeTrackBrowser(browser); });
  BrowserList::AddObserver(this);
}

bool BrowserTabStripTracker::ShouldTrackBrowser(Browser* browser) {
  return !delegate_ || delegate_->ShouldTrackBrowser(browser);
}

void BrowserTabStripTracker::MaybeTrackBrowser(Browser* browser) {
  if (!ShouldTrackBrowser(browser)) {
    return;
  }

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  tab_strip_model->AddObserver(tab_strip_model_observer_);

  TabStripModelChange::Insert insert;
  for (int i = 0; i < tab_strip_model->count(); ++i) {
    insert.contents.push_back({tab_strip_model->GetTabAtIndex(i),
                               tab_strip_model->GetWebContentsAt(i), i});
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
  browser->tab_strip_model()->RemoveObserver(tab_strip_model_observer_);
}
