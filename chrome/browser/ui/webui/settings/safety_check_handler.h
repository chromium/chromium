// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SAFETY_CHECK_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SAFETY_CHECK_HANDLER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/webui/help/version_updater.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check_service.h"
#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"
#include "components/safety_check/safety_check.h"
#include "components/safety_check/update_check_helper.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"

// Delegate for accessing external timestamps, overridden for tests.
class TimestampDelegate {
 public:
  virtual ~TimestampDelegate() = default;
  virtual base::Time GetSystemTime();
};

// Settings page UI handler that checks four areas of browser safety:
// browser updates, password leaks, malicious extensions, and unwanted
// software.
class SafetyCheckHandler
    : public settings::SettingsPageUIHandler,
      public password_manager::BulkLeakCheckServiceInterface::Observer,
      public password_manager::InsecureCredentialsManager::Observer {
 public:
  // The following enum represent the state of the safety check parent
  // component and  should be kept in sync with the JS frontend
  // (safety_check_browser_proxy.js).
  enum class ParentStatus {
    kBefore = 0,
    kChecking = 1,
    kAfter = 2,
  };

  // The following enums represent the state of each component of the safety
  // check and should be kept in sync with the JS frontend
  // (safety_check_browser_proxy.js) and |SafetyCheck*| metrics enums in
  // enums.xml.
  using PasswordsStatus = safety_check::PasswordsStatus;
  using SafeBrowsingStatus = safety_check::SafeBrowsingStatus;
  using UpdateStatus = safety_check::UpdateStatus;
  enum class ExtensionsStatus {
    kChecking = 0,
    // Deprecated in M92, because the unknown extension state is never stored in
    // extension prefs.
    // kError = 1,
    kNoneBlocklisted = 2,
    kBlocklistedAllDisabled = 3,
    kBlocklistedReenabledAllByUser = 4,
    // In this case, at least one of the extensions was re-enabled by admin.
    kBlocklistedReenabledSomeByUser = 5,
    kBlocklistedReenabledAllByAdmin = 6,
    // New enum values must go above here.
    kMaxValue = kBlocklistedReenabledAllByAdmin,
  };

  SafetyCheckHandler();
  ~SafetyCheckHandler() override;

  // Triggers WebUI updates about safety check now running.
  // Note: since the checks deal with sensitive user information, this method
  // should only be called as a result of an explicit user action.
  void SendSafetyCheckStartedWebUiUpdates();

  // Triggers all safety check child checks.
  // Note: since the checks deal with sensitive user information, this method
  // should only be called as a result of an explicit user action.
  void PerformSafetyCheck();

  // Constructs a string depicting how much time passed since the completion of
  // something from the corresponding timestamps and strings IDs.
  std::u16string GetStringForTimePassed(base::Time completion_timestamp,
                                        base::Time system_time,
                                        int less_than_one_minute_ago_message_id,
                                        int minutes_ago_message_id,
                                        int hours_ago_message_id,
                                        int yesterday_message_id,
                                        int days_ago_message_id);

  // Constructs the 'safety check ran' display string by how long ago safety
  // check ran.
  std::u16string GetStringForParentRan(base::Time safety_check_completion_time);
  std::u16string GetStringForParentRan(base::Time safety_check_completion_time,
                                       base::Time system_time);

 protected:
  SafetyCheckHandler(
      std::unique_ptr<safety_check::UpdateCheckHelper> update_helper,
      std::unique_ptr<VersionUpdater> version_updater,
      password_manager::BulkLeakCheckService* leak_service,
      extensions::PasswordsPrivateDelegate* passwords_delegate,
      extensions::ExtensionPrefs* extension_prefs,
      extensions::ExtensionServiceInterface* extension_service,
      std::unique_ptr<TimestampDelegate> timestamp_delegate);

  void SetVersionUpdaterForTesting(
      std::unique_ptr<VersionUpdater> version_updater) {
    version_updater_ = std::move(version_updater);
  }

  void SetTimestampDelegateForTesting(
      std::unique_ptr<TimestampDelegate> timestamp_delegate) {
    timestamp_delegate_ = std::move(timestamp_delegate);
  }

 private:
  // These ensure integers are passed in the correct possitions in the extension
  // check methods.
  using Compromised = base::StrongAlias<class CompromisedTag, int>;
  using Weak = base::StrongAlias<class WeakTag, int>;
  using Reused = base::StrongAlias<class ReusedTag, int>;
  using Done = base::StrongAlias<class DoneTag, int>;
  using Total = base::StrongAlias<class TotalTag, int>;
  using Blocklisted = base::StrongAlias<class BlocklistedTag, int>;
  using ReenabledUser = base::StrongAlias<class ReenabledUserTag, int>;
  using ReenabledAdmin = base::StrongAlias<class ReenabledAdminTag, int>;

  // Handles triggering the safety check from the frontend (by user pressing a
  // button).
  void HandlePerformSafetyCheck(const base::Value::List& args);

  // Handles updating the safety check parent display string to show how long
  // ago the safety check last ran.
  void HandleGetParentRanDisplayString(const base::Value::List& args);

  // Triggers an update check and invokes OnUpdateCheckResult once results
  // are available.
  void CheckUpdates();

  // Triggers a bulk password leak check and invokes OnPasswordsCheckResult once
  // results are available.
  void CheckPasswords();

  // Checks if any of the installed extensions are blocklisted, and in
  // that case, if any of those were re-enabled.
  void CheckExtensions();

  // Callbacks that get triggered when each check completes.
  void OnUpdateCheckResult(UpdateStatus status);
  void OnPasswordsCheckResult(PasswordsStatus status,
                              Compromised compromised,
                              Weak weak,
                              Reused reused,
                              Done done,
                              Total total);
  void OnExtensionsCheckResult(ExtensionsStatus status,
                               Blocklisted blocklisted,
                               ReenabledUser reenabled_user,
                               ReenabledAdmin reenabled_admin);

  // Methods for building user-visible strings based on the safety check
  // state.
  std::u16string GetStringForParent(ParentStatus status);
  std::u16string GetStringForUpdates(UpdateStatus status);
  std::u16string GetStringForSafeBrowsing(SafeBrowsingStatus status);
  std::u16string GetStringForPasswords(PasswordsStatus status,
                                       Compromised compromised,
                                       Weak weak,
                                       Reused reused,
                                       Done done,
                                       Total total);
  std::u16string GetStringForExtensions(ExtensionsStatus status,
                                        Blocklisted blocklisted,
                                        ReenabledUser reenabled_user,
                                        ReenabledAdmin reenabled_admin);

  // A generic error state often includes the offline state. This method is used
  // as a callback for |UpdateCheckHelper| to check connectivity.
  void DetermineIfOfflineOrError(bool connected);

  // Since the password check API does not distinguish between the cases of
  // having no compromised passwords and not having any passwords at all, it is
  // necessary to use this method as a callback for
  // |PasswordsPrivateDelegate::GetSavedPasswordsList| to distinguish the two
  // states here.
  void DetermineIfNoPasswordsOrSafe(
      const std::vector<extensions::api::passwords_private::PasswordUiEntry>&
          passwords);

  // Gets the compromised passwords count and invokes an appropriate result
  // method depending on the state.
  void UpdatePasswordsResultOnCheckIdle();

  // A callback passed to |VersionUpdater::CheckForUpdate| to receive the update
  // state.
  void OnVersionUpdaterResult(VersionUpdater::Status status,
                              int progress,
                              bool rollback,
                              bool powerwash,
                              const std::string& version,
                              int64_t update_size,
                              const std::u16string& message);

  // BulkLeakCheckService::Observer implementation.
  void OnStateChanged(
      password_manager::BulkLeakCheckService::State state) override;
  void OnCredentialDone(const password_manager::LeakCheckCredential& credential,
                        password_manager::IsLeaked is_leaked) override;

  // InsecureCredentialsManager::Observer implementation.
  void OnInsecureCredentialsChanged() override;

  // SettingsPageUIHandler implementation.
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Updates the parent status from the children statuses.
  void CompleteParentIfChildrenCompleted();

  // Fire a safety check element WebUI update with a state and string.
  void FireBasicSafetyCheckWebUiListener(const std::string& event_name,
                                         int new_state,
                                         const std::u16string& display_string);

  // The current status of the safety check elements. Before safety
  // check is started, the parent is in the 'before' state.
  ParentStatus parent_status_ = ParentStatus::kBefore;
  UpdateStatus update_status_ = UpdateStatus::kChecking;
  PasswordsStatus passwords_status_ = PasswordsStatus::kChecking;
  SafeBrowsingStatus safe_browsing_status_ = SafeBrowsingStatus::kChecking;
  ExtensionsStatus extensions_status_ = ExtensionsStatus::kChecking;
  // System time when safety check completed.
  base::Time safety_check_completion_time_;
  // Tracks whether there is at least one |OnCredentialDone| callback with
  // is_leaked = true.
  bool compromised_passwords_exist_ = false;

  std::unique_ptr<safety_check::UpdateCheckHelper> update_helper_;

  std::unique_ptr<VersionUpdater> version_updater_;
  raw_ptr<password_manager::BulkLeakCheckServiceInterface> leak_service_ =
      nullptr;
  raw_ptr<password_manager::InsecureCredentialsManager>
      insecure_credentials_manager_ = nullptr;
  scoped_refptr<extensions::PasswordsPrivateDelegate> passwords_delegate_;
  raw_ptr<extensions::ExtensionPrefs> extension_prefs_ = nullptr;
  raw_ptr<extensions::ExtensionServiceInterface> extension_service_ = nullptr;
  base::ScopedObservation<
      password_manager::BulkLeakCheckServiceInterface,
      password_manager::BulkLeakCheckServiceInterface::Observer>
      observed_leak_check_{this};
  base::ScopedObservation<
      password_manager::InsecureCredentialsManager,
      password_manager::InsecureCredentialsManager::Observer>
      observed_insecure_credentials_manager_{this};
  std::unique_ptr<TimestampDelegate> timestamp_delegate_;
  base::WeakPtrFactory<SafetyCheckHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SAFETY_CHECK_HANDLER_H_
