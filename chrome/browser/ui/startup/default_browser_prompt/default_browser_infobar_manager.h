// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_INFOBAR_MANAGER_H_
#define CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_INFOBAR_MANAGER_H_

#include <map>
#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
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
class DefaultBrowserInfoBarManager : public BrowserTabStripTrackerDelegate,
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

  bool can_pin_to_taskbar_ = false;

  std::unique_ptr<BrowserTabStripTracker> browser_tab_strip_tracker_;
  std::map<content::WebContents*, raw_ptr<infobars::InfoBar, CtnExperimental>>
      infobars_;

  std::optional<DefaultBrowserPromptManager::CloseReason>
      user_initiated_info_bar_close_pending_;
};

#endif  // CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_INFOBAR_MANAGER_H_
