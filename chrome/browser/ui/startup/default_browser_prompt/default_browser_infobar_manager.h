// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_INFOBAR_MANAGER_H_
#define CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_INFOBAR_MANAGER_H_

#include <map>
#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

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

// DefaultBrowserInfobarManager is responsible for  displaying and managing
// Default Browser infobars across all tabs and browser windows. It also acts as
// an observer for InfoBar delegates to record and act on user interaction with
// the infobars.
class DefaultBrowserInfoBarManager : public BrowserListObserver,
                                     public BrowserTabStripTrackerDelegate,
                                     public TabStripModelObserver,
                                     public infobars::InfoBarManager::Observer,
                                     public ConfirmInfoBarDelegate::Observer {
 public:
  DefaultBrowserInfoBarManager();
  ~DefaultBrowserInfoBarManager() override;

  DefaultBrowserInfoBarManager(const DefaultBrowserInfoBarManager&) = delete;
  DefaultBrowserInfoBarManager& operator=(const DefaultBrowserInfoBarManager&) =
      delete;

  void ShowInfoBars(bool can_pin_to_taskbar);
  void CloseAllInfoBars();

 private:
  // Possible user interactions with the default browser info bar.
  // These values are persisted to logs. Entries should not be
  // renumbered and numeric values should never be reused.
  //
  // LINT.IfChange(InfoBarUserInteraction)
  enum InfoBarUserInteraction {
    // The user clicked the "OK" (i.e., "Set as default") button.
    ACCEPT_INFO_BAR = 0,
    // The cancel button is deprecated.
    // CANCEL_INFO_BAR = 1,
    // Deprecated, new actions should be recorded in the
    // `IGNORED_INFO_BAR_PER_SESSION` bucket.
    // IGNORE_INFO_BAR = 2,
    // The user explicitly closed the infobar.
    DISMISS_INFO_BAR = 3,
    // The user did not interact with the infobar before closing the last
    // browser window.
    IGNORE_INFO_BAR_PER_SESSION = 4,
    NUM_INFO_BAR_USER_INTERACTION_TYPES
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/ui/enums.xml:DefaultBrowserInfoBarUserInteraction)

  void CreateInfoBarForWebContents(content::WebContents* contents,
                                   Profile* profile);

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;

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

  bool can_pin_to_taskbar_ = false;

  std::unique_ptr<BrowserTabStripTracker> browser_tab_strip_tracker_;
  std::map<content::WebContents*, raw_ptr<infobars::InfoBar, CtnExperimental>>
      infobars_;

  std::optional<DefaultBrowserPromptManager::CloseReason>
      user_initiated_info_bar_close_pending_;

  base::ScopedObservation<BrowserList, BrowserListObserver>
      browser_list_observation_{this};
};

#endif  // CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_INFOBAR_MANAGER_H_
