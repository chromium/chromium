// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_STRIP_INTERNALS_TAB_STRIP_INTERNALS_OBSERVER_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_STRIP_INTERNALS_TAB_STRIP_INTERNALS_OBSERVER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/sessions/core/tab_restore_service_observer.h"

class Browser;
class BrowserWindowInterface;
class Profile;
class TabStripModel;

// Observes tab strip–related events and notifies clients when something
// changes.
class TabStripInternalsObserver : public BrowserListObserver,
                                  public TabStripModelObserver,
                                  public sessions::TabRestoreServiceObserver {
 public:
  using UpdateCallback = base::RepeatingCallback<void()>;

  explicit TabStripInternalsObserver(Profile* profile, UpdateCallback callback);

  TabStripInternalsObserver(const TabStripInternalsObserver&) = delete;
  TabStripInternalsObserver& operator=(const TabStripInternalsObserver&) =
      delete;
  ~TabStripInternalsObserver() override;

  // BrowserListObserver methods
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // TabStripModelObserver methods
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnTabGroupChanged(const TabGroupChange& change) override;
  void OnSplitTabChanged(const SplitTabChange& change) override;
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;
  void TabPinnedStateChanged(TabStripModel* tab_strip_model,
                             content::WebContents* contents,
                             int index) override;
  void TabBlockedStateChanged(content::WebContents* contents,
                              int index) override;
  void TabGroupedStateChanged(TabStripModel* tab_strip_model,
                              std::optional<tab_groups::TabGroupId> old_group,
                              std::optional<tab_groups::TabGroupId> new_group,
                              tabs::TabInterface* tab,
                              int index) override;

  // TabRestoreServiceObserver methods.
  void TabRestoreServiceChanged(sessions::TabRestoreService* service) override;
  void TabRestoreServiceDestroyed(
      sessions::TabRestoreService* service) override;

 private:
  // Add this as an observer to a browser's TabStripModel.
  void StartObservingBrowser(BrowserWindowInterface* browser);
  // Remove this as an observer from a browser's TabStripModel.
  void StopObservingBrowser(BrowserWindowInterface* browser);
  // Add this an observer to the TabRestoreService for the given profile.
  void StartObservingTabRestore(Profile* profile);
  // Remove this as an observer from the currently tracked TabRestoreService.
  void StopObservingTabRestore();
  // Notify the client that something has changed.
  void FireUpdate();

  // The TabRestoreService instance currently being observed.
  raw_ptr<sessions::TabRestoreService> service_ = nullptr;
  UpdateCallback callback_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_STRIP_INTERNALS_TAB_STRIP_INTERNALS_OBSERVER_H_
