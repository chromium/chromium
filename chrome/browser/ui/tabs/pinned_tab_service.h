// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_PINNED_TAB_SERVICE_H_
#define CHROME_BROWSER_UI_TABS_PINNED_TAB_SERVICE_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

// PinnedTabService is responsible for updating preferences with the set of
// pinned tabs to restore at startup. PinnedTabService listens for the
// appropriate set of notifications to know it should update preferences.
class PinnedTabService : public BrowserListObserver,
                         public TabStripModelObserver,
                         public KeyedService {
 public:
  explicit PinnedTabService(Profile* profile);
  PinnedTabService(const PinnedTabService&) = delete;
  PinnedTabService& operator=(const PinnedTabService&) = delete;
  ~PinnedTabService() override;

 private:
  // Invoked with true when all browsers start closing.
  void OnClosingAllBrowsersChanged(bool closing);

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserClosing(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // Writes the pinned tabs for |profile_|, but only if a new tab or browser
  // window has been added since the last time the method was called.
  void WritePinnedTabsIfNecessary();

  raw_ptr<Profile> profile_;

  // True if we should save the pinned tabs when a browser window closes or the
  // user exits the application. This is set to false after writing pinned tabs,
  // and set back to true when new tabs or windows are added.
  bool need_to_write_pinned_tabs_ = true;

  base::CallbackListSubscription closing_all_browsers_subscription_;
};

#endif  // CHROME_BROWSER_UI_TABS_PINNED_TAB_SERVICE_H_
