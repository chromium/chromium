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
#include "chrome/browser/default_browser/default_browser_controller.h"
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_surface_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace content {
class WebContents;
}  // namespace content

namespace default_browser {
class DefaultBrowserController;
}  // namespace default_browser

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
class DefaultBrowserInfoBarManager : public BrowserCollectionObserver,
                                     public BrowserTabStripTrackerDelegate,
                                     public DefaultBrowserSurfaceManager,
                                     public TabStripModelObserver,
                                     public infobars::InfoBarManager::Observer,
                                     public ConfirmInfoBarDelegate::Observer {
 public:
  DefaultBrowserInfoBarManager();
  ~DefaultBrowserInfoBarManager() override;

  DefaultBrowserInfoBarManager(const DefaultBrowserInfoBarManager&) = delete;
  DefaultBrowserInfoBarManager& operator=(const DefaultBrowserInfoBarManager&) =
      delete;

  // DefaultBrowserSurfaceManager:
  void Show(
      std::unique_ptr<default_browser::DefaultBrowserController> controller,
      bool can_pin_to_taskbar) override;
  void CloseAll() override;
  default_browser::DefaultBrowserEntrypointType GetEntrypointType()
      const override;

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

  // BrowserCollectionObserver:
  void OnBrowserClosed(BrowserWindowInterface* browser) override;

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

  // DefaultBrowserController is created when the this class is requested to
  // show inforbars. Destruction is handled for all 3 possible cases:
  //   1. User accepted: Controller is destroyed once the process of setting
  //      Chrome as default completes.
  //   2. User declined: Controller is destroyed immediately.
  //   3. User ignored: Controller is destroyed immediately.
  std::unique_ptr<default_browser::DefaultBrowserController>
      default_browser_controller_;

  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
};

#endif  // CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_INFOBAR_MANAGER_H_
