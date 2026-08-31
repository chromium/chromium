// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SESSION_RESTORE_INFOBAR_SESSION_RESTORE_INFOBAR_MANAGER_H_
#define CHROME_BROWSER_UI_VIEWS_SESSION_RESTORE_INFOBAR_SESSION_RESTORE_INFOBAR_MANAGER_H_

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/memory/singleton.h"
#include "chrome/browser/infobars/infobar_spec.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
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

// Enum for the message type to be displayed in the infobar.
enum class InfobarMessageType {
  kNone,
  // Infobar message displayed for turning off session restore from restart.
  kTurnOffFromRestart,
  // Infobar message displayed for turning on session restore.
  kTurnOnSessionRestore,
};

// Enum for session restore infobar actions.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SessionRestoreInfoBarAction)
enum class InfobarAction {
  kShown = 0,
  kDismissed = 1,
  kLinkClicked = 2,
  kIgnored = 3,
  kMaxValue = kIgnored,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/session/enums.xml:SessionRestoreInfoBarAction)

// Records UMA metrics for session restore infobar actions.
void RecordInfoBarAction(InfobarMessageType message_type, InfobarAction action);

// Records UMA metrics for session restore setting changes.
void RecordSettingChanged(bool setting_changed,
                          InfobarMessageType message_type);

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
  void ShowInfoBar(Profile& profile, InfobarMessageType message_type);

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

  // Returns the localized message text corresponding to the current message type.
  std::u16string GetMessageText() const;

  // Message substitution helper for centralized infobar registration.
  std::vector<MessageSubstitution> GetMessageSubstitutions() const;

  // Called by the centralized infobar framework on terminal outcomes.
  void OnInfoBarResult(infobars::InfoBarResult result);

  // BrowserTabStripTrackerDelegate
  bool ShouldTrackBrowser(BrowserWindowInterface* browser) override;

 private:
  friend struct base::DefaultSingletonTraits<SessionRestoreInfoBarManager>;

  SessionRestoreInfoBarManager();
  ~SessionRestoreInfoBarManager() override;

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
  InfobarMessageType message_type_ = InfobarMessageType::kNone;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  bool shown_metric_recorded_for_session_ = false;
  bool ignored_metric_recorded_for_session_ = false;
  bool action_taken_for_session_ = false;
};

}  // namespace session_restore_infobar

#endif  // CHROME_BROWSER_UI_VIEWS_SESSION_RESTORE_INFOBAR_SESSION_RESTORE_INFOBAR_MANAGER_H_
