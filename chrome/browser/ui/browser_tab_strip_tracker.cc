// Copyright 2015 The Chromium Authors. All rights reserved.
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
    BrowserTabStripTrackerDelegate* delegate,
    BrowserListObserver* browser_list_observer)
    : tab_strip_model_observer_(tab_strip_model_observer),
      delegate_(delegate),
      browser_list_observer_(browser_list_observer),
      is_processing_initial_browsers_(false) {
  DCHECK(tab_strip_model_observer_);
}

BrowserTabStripTracker::~BrowserTabStripTracker() {
  for (Browser* browser : browsers_observing_)
    browser->tab_strip_model()->RemoveObserver(tab_strip_model_observer_);

  BrowserList::RemoveObserver(this);
}

void BrowserTabStripTracker::Init() {
  BrowserList::AddObserver(this);

  base::AutoReset<bool> restter(&is_processing_initial_browsers_, true);
  for (auto* browser : *BrowserList::GetInstance())
    MaybeTrackBrowser(browser);
}

void BrowserTabStripTracker::StopObservingAndSendOnBrowserRemoved() {
  Browsers current_browsers;
  current_browsers.swap(browsers_observing_);

  for (Browser* browser : current_browsers) {
    browser->tab_strip_model()->RemoveObserver(tab_strip_model_observer_);
    if (browser_list_observer_)
      browser_list_observer_->OnBrowserRemoved(browser);
  }
}

bool BrowserTabStripTracker::ShouldTrackBrowser(Browser* browser) {
  return !delegate_ || delegate_->ShouldTrackBrowser(browser);
}

void BrowserTabStripTracker::MaybeTrackBrowser(Browser* browser) {
  if (!ShouldTrackBrowser(browser))
    return;

  // It's possible that a browser is added to the observed browser list twice.
  // In this case it might cause crash as seen in crbug.com/685731.
  if (browsers_observing_.find(browser) != browsers_observing_.end())
    return;

  browsers_observing_.insert(browser);

  if (browser_list_observer_)
    browser_list_observer_->OnBrowserAdded(browser);

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
  MaybeTrackBrowser(browser);
}

void BrowserTabStripTracker::OnBrowserRemoved(Browser* browser) {
  auto it = browsers_observing_.find(browser);
  if (it == browsers_observing_.end())
    return;

  browsers_observing_.erase(it);
  browser->tab_strip_model()->RemoveObserver(tab_strip_model_observer_);

  if (browser_list_observer_)
    browser_list_observer_->OnBrowserRemoved(browser);
}

void BrowserTabStripTracker::OnBrowserSetLastActive(Browser* browser) {
  if (browser_list_observer_)
    browser_list_observer_->OnBrowserSetLastActive(browser);
}
