// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/pinned_tab_service.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/pinned_tab_codec.h"
#include "content/public/browser/notification_service.h"

PinnedTabService::PinnedTabService(Profile* profile) : profile_(profile) {
  registrar_.Add(this, chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST,
                 content::NotificationService::AllSources());

  for (Browser* browser : *BrowserList::GetInstance())
    OnBrowserAdded(browser);

  BrowserList::AddObserver(this);
}

PinnedTabService::~PinnedTabService() {
  BrowserList::RemoveObserver(this);
}

void PinnedTabService::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
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
  DCHECK_EQ(type, chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST);
  if (TabStripModelObserver::IsObservingAny(this))
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
