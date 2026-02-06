// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_STARTUP_LAUNCH_INFOBAR_MANAGER_IMPL_H_
#define CHROME_BROWSER_UI_STARTUP_STARTUP_LAUNCH_INFOBAR_MANAGER_IMPL_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/startup/startup_launch_infobar_manager.h"
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar_manager.h"

namespace content {
class WebContents;
}  // namespace content

namespace infobars {
class InfoBar;
}  // namespace infobars

class BrowserTabStripTracker;
class BrowserWindowInterface;

class Profile;

class TabStripModel;

// UMA enum for tracking interactions with Startup Launch infobars.
// These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused.
//
// LINT.IfChange(StartupLaunchInfoBarInteraction)
enum class StartupLaunchInfoBarInteraction {
  kDismiss = 0,
  kAccept = 1,
  kMaxValue = kAccept,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/startup/histograms.xml:StartupLaunchInfoBarInteraction)

// Implementation of StartupLaunchInfoBarManager that manages the lifecycle of
// startup launch infobars across multiple browser windows and tabs.
// It tracks tab insertions to show the infobar on new tabs and observes
// infobar events to handle user interactions and cleanup.
class StartupLaunchInfoBarManagerImpl
    : public StartupLaunchInfoBarManager,
      public BrowserCollectionObserver,
      public BrowserTabStripTrackerDelegate,
      public TabStripModelObserver,
      public infobars::InfoBarManager::Observer,
      public ConfirmInfoBarDelegate::Observer {
 public:
  StartupLaunchInfoBarManagerImpl();
  ~StartupLaunchInfoBarManagerImpl() override;

  StartupLaunchInfoBarManagerImpl(const StartupLaunchInfoBarManagerImpl&) =
      delete;
  StartupLaunchInfoBarManagerImpl& operator=(
      const StartupLaunchInfoBarManagerImpl&) = delete;

  // StartupLaunchInfoBarManager:
  void ShowInfoBars(InfoBarType infobar_type) override;
  void CloseAllInfoBars() override;
  void AddObserver(StartupLaunchInfoBarManager::Observer* observer) override;
  void RemoveObserver(StartupLaunchInfoBarManager::Observer* observer) override;

 private:
  void CreateInfoBarForWebContents(content::WebContents* contents,
                                   Profile* profile);

  // BrowserTabStripTrackerDelegate
  bool ShouldTrackBrowser(BrowserWindowInterface* browser) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // InfoBarManager::Observer:
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;

  // ConfirmInfoBarDelegate::Observer
  void OnAccept() override;
  void OnDismiss() override;

  std::unique_ptr<BrowserTabStripTracker> browser_tab_strip_tracker_;
  std::map<raw_ptr<content::WebContents>, raw_ptr<infobars::InfoBar>> infobars_;

  base::ObserverList<StartupLaunchInfoBarManager::Observer> observers_;

  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};

  bool did_user_interact_ = false;
  InfoBarType infobar_type_;
};

#endif  // CHROME_BROWSER_UI_STARTUP_STARTUP_LAUNCH_INFOBAR_MANAGER_IMPL_H_
