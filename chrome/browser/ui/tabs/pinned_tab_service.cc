// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/pinned_tab_service.h"

#include "base/functional/bind.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/pinned_tab_codec.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

PinnedTabService::PinnedTabService(Profile* profile) : profile_(profile) {
  closing_all_browsers_subscription_ = chrome::AddClosingAllBrowsersCallback(
      base::BindRepeating(&PinnedTabService::OnClosingAllBrowsersChanged,
                          base::Unretained(this)));

  for (Browser* browser : *BrowserList::GetInstance())
    OnBrowserAdded(browser);

  BrowserList::AddObserver(this);
}

PinnedTabService::~PinnedTabService() {
  BrowserList::RemoveObserver(this);
}

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
  if (closing && TabStripModelObserver::IsObservingAny(this))
    WritePinnedTabsIfNecessary();
}

void PinnedTabService::OnBrowserAdded(Browser* browser) {
  if (browser->profile() != profile_ || !browser->is_type_normal())
    return;

  need_to_write_pinned_tabs_ = true;
  browser->tab_strip_model()->AddObserver(this);
}

void PinnedTabService::OnBrowserClosing(Browser* browser) {
  if (browser->profile() != profile_ || !browser->is_type_normal())
    return;

  if (TabStripModelObserver::CountObservedModels(this) == 1)
    WritePinnedTabsIfNecessary();
}

void PinnedTabService::OnBrowserRemoved(Browser* browser) {
  if (browser->profile() != profile_ || !browser->is_type_normal())
    return;

  browser->tab_strip_model()->RemoveObserver(this);

  // This happens when user closes each tabs manually via the close button on
  // them. In this case OnBrowserClosing() above is not called. This causes
  // pinned tabs to repopen on the next startup. So we should call
  // WritePinnedTab() to clear the data.
  // http://crbug.com/71939
  if (!TabStripModelObserver::IsObservingAny(this))
    WritePinnedTabsIfNecessary();
}

void PinnedTabService::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kInserted)
    need_to_write_pinned_tabs_ = true;
}

void PinnedTabService::WritePinnedTabsIfNecessary() {
  if (need_to_write_pinned_tabs_)
    PinnedTabCodec::WritePinnedTabs(profile_);
  need_to_write_pinned_tabs_ = false;
}
