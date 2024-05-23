// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_MANAGER_H_

#include <memory>

#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/password_manager/core/browser/password_reuse_detector_consumer.h"
#include "components/password_manager/core/browser/shared_preferences_delegate.h"

namespace signin {
class IdentityManager;
}  // namespace signin

class PrefService;

namespace password_manager {

class PasswordManagerClient;
class PasswordStoreInterface;
class PasswordReuseManagerSigninNotifier;
struct PasswordForm;

// Per-store class responsible for detection of password reuse, i.e. that the
// user input on some site contains the password saved on another site.
class PasswordReuseManager : public KeyedService {
 public:
  PasswordReuseManager() = default;

  PasswordReuseManager(const PasswordReuseManager&) = delete;
  PasswordReuseManager& operator=(const PasswordReuseManager&) = delete;

  // Always call this on the UI thread.
  virtual void Init(
      PrefService* prefs,
      PrefService* local_prefs,
      PasswordStoreInterface* profile_store,
      PasswordStoreInterface* account_store,
      std::unique_ptr<PasswordReuseDetector> password_reuse_detector,
      signin::IdentityManager* identity_manager = nullptr,
      std::unique_ptr<SharedPreferencesDelegate> shared_pref_delegate =
          nullptr) = 0;

  // Log whether a sync password hash saved.
  virtual void ReportMetrics(const std::string& username) = 0;

  // Checks that some suffix of |input| equals to a password saved on another
  // registry controlled domain than |domain|.
  // If such suffix is found, |consumer|->OnReuseFound() is called on the main
  // sequence.
  // |consumer| must not be null.
  virtual void CheckReuse(const std::u16string& input,
                          const std::string& domain,
                          PasswordReuseDetectorConsumer* consumer) = 0;

  // Saves |username| and a hash of |password| for GAIA password reuse checking.
  // |event| is used for metric logging and for distinguishing sync password
  // hash change event and other non-sync GAIA password change event.
  // |is_sync_password_for_metrics| is whether the password belongs to the
  // primary account with sync the feature enabled, used for metrics only.
  virtual void SaveGaiaPasswordHash(
      const std::string& username,
      const std::u16string& password,
      bool is_sync_password_for_metrics,
      metrics_util::GaiaPasswordHashChange event) = 0;

  // Saves |username| and a hash of |password| for enterprise password reuse
  // checking.
  virtual void SaveEnterprisePasswordHash(const std::string& username,
                                          const std::u16string& password) = 0;

  // Saves |sync_password_data| for sync password reuse checking.
  // |event| is used for metric logging.
  virtual void SaveSyncPasswordHash(
      const PasswordHashData& sync_password_data,
      metrics_util::GaiaPasswordHashChange event) = 0;

  // Clears the saved GAIA password hash for |username|.
  virtual void ClearGaiaPasswordHash(const std::string& username) = 0;

  // Clears all the GAIA password hash.
  virtual void ClearAllGaiaPasswordHash() = 0;

  // Clears all (non-GAIA) enterprise password hash.
  virtual void ClearAllEnterprisePasswordHash() = 0;

  // Clear all GAIA password hash that is not associated with a Gmail account.
  virtual void ClearAllNonGmailPasswordHash() = 0;

  // Adds a listener on |hash_password_manager_| for when |kHashPasswordData|
  // list might have changed. Should only be called on the UI thread.
  virtual base::CallbackListSubscription
  RegisterStateCallbackOnHashPasswordManager(
      const base::RepeatingCallback<void(const std::string& username)>&
          callback) = 0;

  // Shouldn't be called more than once, |notifier| must be not nullptr.
  virtual void SetPasswordReuseManagerSigninNotifier(
      std::unique_ptr<PasswordReuseManagerSigninNotifier> notifier) = 0;

  // Schedules the update of enterprise login and change password URLs.
  // These URLs are used in enterprise password reuse detection.
  virtual void ScheduleEnterprisePasswordURLUpdate() = 0;

  // Saves the hash version of a password if it corresponds to an
  // enterprise or gaia password.
  virtual void MaybeSavePasswordHash(const PasswordForm* submitted_form,
                                     PasswordManagerClient* client) = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_MANAGER_H_
