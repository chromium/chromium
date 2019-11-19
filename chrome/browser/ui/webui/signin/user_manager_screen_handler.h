// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_USER_MANAGER_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_USER_MANAGER_SCREEN_HANDLER_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_statistics.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "google_apis/gaia/gaia_oauth_client.h"

class Browser;

namespace base {
class DictionaryValue;
class FilePath;
class ListValue;
}

extern const char kAuthenticatedLaunchUserEventMetricsName[];

// UI event when user click a locked profile. It must matches the
// AuthenticatedLaunchUserEvent in enums.xml
enum AuthenticatedLaunchUserEvent {
  LOCAL_REAUTH_DIALOG,
  GAIA_REAUTH_DIALOG,
  SUPERVISED_PROFILE_BLOCKED_WARNING,
  USED_PROFILE_BLOCKED_WARNING,
  FORCED_PRIMARY_SIGNIN_DIALOG,
  EVENT_COUNT,
};

class UserManagerScreenHandler : public content::WebUIMessageHandler,
                                 public BrowserListObserver,
                                 public gaia::GaiaOAuthClient::Delegate {
 public:
  UserManagerScreenHandler();
  ~UserManagerScreenHandler() override;

  void GetLocalizedValues(base::DictionaryValue* localized_strings);

 private:
  // An observer for any changes to Profiles in the ProfileAttributesStorage so
  // that all the visible user manager screens can be updated.
  class ProfileUpdateObserver;

  // WebUIMessageHandler:
  void RegisterMessages() override;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;

  void HandleInitialize(const base::ListValue* args);
  void HandleAuthenticatedLaunchUser(const base::ListValue* args);
  void HandleLaunchGuest(const base::ListValue* args);
  void HandleLaunchUser(const base::ListValue* args);
  void HandleRemoveUser(const base::ListValue* args);
  void HandleAreAllProfilesLocked(const base::ListValue* args);
  void HandleRemoveUserWarningLoadStats(const base::ListValue* args);

  // Function used to gather statistics from a profile.
  void GatherStatistics(base::Time start_time, Profile* profile);

  // Callback function used by HandleRemoveUserWarningLoadStats
  void RemoveUserDialogLoadStatsCallback(base::FilePath profile_path,
                                         base::Time start_time,
                                         profiles::ProfileCategoryStats result);

  // gaia::GaiaOAuthClient::Delegate:
  void OnGetTokenInfoResponse(
      std::unique_ptr<base::DictionaryValue> token_info) override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

  // Sends user list to account chooser.
  void SendUserList();

  // Pass success/failure information back to the web page.
  void ReportAuthenticationResult(bool success,
                                  ProfileMetrics::ProfileAuth metric);

  // Perform cleanup once the profile and browser are open.
  void OnSwitchToProfileComplete(Profile* profile,
                                 Profile::CreateStatus profile_create_status);

  // Observes the ProfileAttributesStorage and gets notified when a profile has
  // been modified, so that the displayed user pods can be updated.
  std::unique_ptr<ProfileUpdateObserver> profile_attributes_storage_observer_;

  // Authenticator used when local-auth fails.
  std::unique_ptr<gaia::GaiaOAuthClient> oauth_client_;

  // The path of the profile currently being authenticated.
  base::FilePath authenticating_profile_path_;

  // Login email held during on-line auth for later use.
  std::string email_address_;

  // URL hash, used to key post-profile actions if present.
  std::string url_hash_;

  // The CancelableTaskTracker is currently used by GetProfileStatistics
  base::CancelableTaskTracker tracker_;

  base::WeakPtrFactory<UserManagerScreenHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UserManagerScreenHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_USER_MANAGER_SCREEN_HANDLER_H_
