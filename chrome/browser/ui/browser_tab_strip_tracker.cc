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
  // Per ObserverList::RemoveObserver() documentation, this does nothing if the
  // observer is not in the ObserverList (i.e. if |browser| is not tracked).
  for (Browser* browser : *BrowserList::GetInstance()) {
    browser->tab_strip_model()->RemoveObserver(tab_strip_model_observer_);
  }

  BrowserList::RemoveObserver(this);
}

void BrowserTabStripTracker::Init() {
  BrowserList::AddObserver(this);

  base::AutoReset<bool> resetter(&is_processing_initial_browsers_, true);
  for (Browser* browser : *BrowserList::GetInstance()) {
    MaybeTrackBrowser(browser);
  }
}

bool BrowserTabStripTracker::ShouldTrackBrowser(Browser* browser) {
  return !delegate_ || delegate_->ShouldTrackBrowser(browser);
}

void BrowserTabStripTracker::MaybeTrackBrowser(Browser* browser) {
  if (!ShouldTrackBrowser(browser))
    return;

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  tab_strip_model->AddObserver(tab_strip_model_observer_);

  TabStripModelChange::Insert insert;
  for (int i = 0; i < tab_strip_model->count(); ++i) {
    insert.contents.push_back({tab_strip_model->GetWebContentsAt(i), i});
  }

  TabStripModelChange change(std::move(insert));
  TabStripSelectionChange selection(tab_strip_model->GetActiveWebContents(),
                                    tab_strip_model->selection_model());
  tab_strip_model_observer_->OnTabStripModelChanged(tab_strip_model, change,
                                                    selection);
}

void BrowserTabStripTracker::OnBrowserAdded(Browser* browser) {
  // No Browser should be added when iterating on Browsers in Init(), as that
  // may invalidate the iterator.
  DCHECK(!is_processing_initial_browsers_);

  MaybeTrackBrowser(browser);
}

void BrowserTabStripTracker::OnBrowserRemoved(Browser* browser) {
  // No Browser should be removed when iterating on Browsers in Init(), as that
  // invalidates any iterator that is past the removed Browser.
  DCHECK(!is_processing_initial_browsers_);

  // Per ObserverList::RemoveObserver() documentation, this does nothing if the
  // observer is not in the ObserverList (i.e. if |browser| is not tracked).
  browser->tab_strip_model()->RemoveObserver(tab_strip_model_observer_);
}
