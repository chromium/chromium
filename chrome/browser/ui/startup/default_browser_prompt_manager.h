// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_MANAGER_H_
#define CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_MANAGER_H_

#include <map>
#include <string>

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

  static DefaultBrowserPromptManager* GetInstance();

  // Enrolls this client with a synthetic field trial based on the Finch params.
  // Should be called when the default browser prompt is potentially shown, then
  // the client needs to register again on each process startup by calling
  // `EnsureStickToDefaultBrowserPromptCohort()`.
  static void MaybeJoinDefaultBrowserPromptCohort();

  // Ensures that the user's experiment group is appropriately reported to track
  // the effect of the default browser prompt over time. Should be called once
  // per browser process startup.
  static void EnsureStickToDefaultBrowserPromptCohort();

  DefaultBrowserPromptManager();
  ~DefaultBrowserPromptManager() override;

  void ShowPrompt();
  void CreateInfoBarForWebContents(content::WebContents* contents,
                                   Profile* profile);
  void CloseAllInfoBars();

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
  // Reports to the launch study for the default browser prompt synthetic trial.
  static void RegisterSyntheticFieldTrial(const std::string& group_name);

  std::unique_ptr<BrowserTabStripTracker> browser_tab_strip_tracker_;

  std::map<content::WebContents*, infobars::InfoBar*> infobars_;

  bool user_initiated_close_pending_ = false;
};

#endif  // CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_MANAGER_H_
