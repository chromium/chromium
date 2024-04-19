// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_MANAGER_H_
#define CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_MANAGER_H_

#include <map>

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

  // Resets the tracking preferences for the default browser prompts so that
  // they are re-shown if the browser ceases to be the user's chosen default.
  static void ResetPromptPrefs(Profile* profile);

  // Updates the tracking preferences for the default browser prompts to reflect
  // that the prompt was just dismissed. This will ensure the proper delay
  // before re-prompting.
  static void UpdatePrefsForDismissedPrompt(Profile* profile);

  // If enough time has passed since the first show time, the app menu should
  // implicitly be dismissed, in which case prompts will not be shown when
  // `MaybeShowPrompt()` is called.
  static void MaybeResetAppMenuPromptPrefs(Profile* profile);

  bool get_show_app_menu_prompt() const { return show_app_menu_prompt_; }

  bool get_show_app_menu_item() const { return show_app_menu_item_; }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void MaybeShowPrompt();

  void CloseAllPrompts(CloseReason close_reason);

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

 private:
  friend struct base::DefaultSingletonTraits<DefaultBrowserPromptManager>;

  DefaultBrowserPromptManager();
  ~DefaultBrowserPromptManager() override;

  // Whether prompts should be shown based on the last declined time/count prefs
  // and the recurrence feature params.
  static bool ShouldShowPrompts();

  static bool ShouldShowAppMenuPrompt();

  void CreateInfoBarForWebContents(content::WebContents* contents,
                                   Profile* profile);

  void CloseAllInfoBars();

  void SetShowAppMenuPromptVisibility(bool show);

  void SetAppMenuItemVisibility(bool show);

  std::unique_ptr<BrowserTabStripTracker> browser_tab_strip_tracker_;
  std::map<content::WebContents*, infobars::InfoBar*> infobars_;

  std::optional<CloseReason> user_initiated_info_bar_close_pending_;

  bool show_app_menu_prompt_ = false;
  bool show_app_menu_item_ = false;

  base::ObserverList<Observer> observers_;

  base::OneShotTimer app_menu_prompt_dismiss_timer_;
};

#endif  // CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_MANAGER_H_
