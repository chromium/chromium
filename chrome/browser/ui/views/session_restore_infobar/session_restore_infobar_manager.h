// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SESSION_RESTORE_INFOBAR_SESSION_RESTORE_INFOBAR_MANAGER_H_
#define CHROME_BROWSER_UI_VIEWS_SESSION_RESTORE_INFOBAR_SESSION_RESTORE_INFOBAR_MANAGER_H_

#include <map>
#include <memory>
#include <optional>

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_delegate.h"
#include "components/infobars/core/infobar_manager.h"

class Profile;
class BrowserTabStripTracker;

namespace content {
class WebContents;
}

namespace infobars {
class InfoBar;
}

class PrefChangeRegistrar;

namespace session_restore_infobar {

// This class is responsible for managing the session restore infobar. It
// ensures the infobar is shown consistently across all applicable browser
// windows and tabs for a given profile and that interacting with one infobar
// dismisses them all.
class SessionRestoreInfoBarManager : public BrowserTabStripTrackerDelegate,
                                     public TabStripModelObserver,
                                     public infobars::InfoBarManager::Observer,
                                     public ProfileObserver {
 public:
  static SessionRestoreInfoBarManager* GetInstance();

  SessionRestoreInfoBarManager(const SessionRestoreInfoBarManager&) = delete;
  SessionRestoreInfoBarManager& operator=(const SessionRestoreInfoBarManager&) =
      delete;

  // Shows a session restore infobar for the given profile.
  void ShowInfoBar(
      Profile& profile,
      SessionRestoreInfoBarDelegate::InfobarMessageType message_type);

  // Closes all visible session restore infobars.
  void CloseAllInfoBars();

  bool shown_metric_recorded_for_session() const {
    return shown_metric_recorded_for_session_;
  }

  void set_shown_metric_recorded_for_session(bool value) {
    shown_metric_recorded_for_session_ = value;
  }

  bool ignored_metric_recorded_for_session() const {
    return ignored_metric_recorded_for_session_;
  }

  void set_ignored_metric_recorded_for_session(bool value) {
    ignored_metric_recorded_for_session_ = value;
  }

  bool action_taken_for_session() const { return action_taken_for_session_; }

  void set_action_taken_for_session(bool value) {
    action_taken_for_session_ = value;
  }

 private:
  friend struct base::DefaultSingletonTraits<SessionRestoreInfoBarManager>;

  SessionRestoreInfoBarManager();
  ~SessionRestoreInfoBarManager() override;

  // BrowserTabStripTrackerDelegate
  bool ShouldTrackBrowser(BrowserWindowInterface* browser) override;
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // infobars::InfoBarManager::Observer:
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // Callback for session restore preference changes.
  void OnSessionRestorePreferenceChanged();

  // Helper methods
  void InitTabStripTracker();
  void CreateInfoBarForWebContents(content::WebContents* web_contents);
  void OnUserInitiatedInfoBarClose();

  // The profile for which the infobar is currently shown.
  raw_ptr<Profile> profile_ = nullptr;
  std::unique_ptr<BrowserTabStripTracker> browser_tab_strip_tracker_ = nullptr;
  // Tracks all infobars created by this controller.
  std::map<content::WebContents*, infobars::InfoBar*> infobars_;
  // Stores whether a user-initiated close is pending, which triggers closing
  // all other infobars.
  bool user_initiated_info_bar_close_pending_ = false;
  SessionRestoreInfoBarDelegate::InfobarMessageType message_type_ =
      SessionRestoreInfoBarDelegate::InfobarMessageType::kNone;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  bool shown_metric_recorded_for_session_ = false;
  bool ignored_metric_recorded_for_session_ = false;
  bool action_taken_for_session_ = false;
};

}  // namespace session_restore_infobar

#endif  // CHROME_BROWSER_UI_VIEWS_SESSION_RESTORE_INFOBAR_SESSION_RESTORE_INFOBAR_MANAGER_H_
