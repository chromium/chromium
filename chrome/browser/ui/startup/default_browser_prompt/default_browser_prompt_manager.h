// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_PROMPT_MANAGER_H_
#define CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_PROMPT_MANAGER_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/timer/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "content/public/browser/web_contents.h"

class DefaultBrowserPromptManager : public BrowserTabStripTrackerDelegate,
                                    public TabStripModelObserver,
                                    public infobars::InfoBarManager::Observer,
                                    public ConfirmInfoBarDelegate::Observer {
 public:
  DefaultBrowserPromptManager(const DefaultBrowserPromptManager&) = delete;
  DefaultBrowserPromptManager& operator=(const DefaultBrowserPromptManager&) =
      delete;

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnShowAppMenuPromptChanged() = 0;
  };

  enum class CloseReason {
    kAccept,
    kDismiss,
  };

  static DefaultBrowserPromptManager* GetInstance();

  bool get_show_app_menu_prompt() const { return show_app_menu_prompt_; }

  bool get_show_app_menu_item() const { return show_app_menu_item_; }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void MaybeShowPrompt();

  void CloseAllPrompts(CloseReason close_reason);

 private:
  friend struct base::DefaultSingletonTraits<DefaultBrowserPromptManager>;

  DefaultBrowserPromptManager();
  ~DefaultBrowserPromptManager() override;

  void CreateInfoBarForWebContents(content::WebContents* contents,
                                   Profile* profile);

  void CloseAllInfoBars();

  void SetShowAppMenuPromptVisibility(bool show);

  void SetAppMenuItemVisibility(bool show);

  // BrowserTabStripTrackerDelegate
  bool ShouldTrackBrowser(Browser* browser) override;

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
  std::map<content::WebContents*, raw_ptr<infobars::InfoBar, CtnExperimental>>
      infobars_;

  std::optional<CloseReason> user_initiated_info_bar_close_pending_;

  bool show_app_menu_prompt_ = false;
  bool show_app_menu_item_ = false;

  base::ObserverList<Observer> observers_;

  base::OneShotTimer app_menu_prompt_dismiss_timer_;
};

#endif // CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_PROMPT_MANAGER_H_
