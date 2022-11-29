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

class PrefService;

namespace password_manager {

class PasswordStoreInterface;
class PasswordStoreSigninNotifier;

using PasswordHashDataList = absl::optional<std::vector<PasswordHashData>>;
using metrics_util::GaiaPasswordHashChange;

// Per-store class responsible for detection of password reuse, i.e. that the
// user input on some site contains the password saved on another site.
class PasswordReuseManager : public KeyedService {
 public:
  PasswordReuseManager() = default;

  PasswordReuseManager(const PasswordReuseManager&) = delete;
  PasswordReuseManager& operator=(const PasswordReuseManager&) = delete;

  // Always call this on the UI thread.
  virtual void Init(PrefService* prefs,
                    PasswordStoreInterface* profile_store,
                    PasswordStoreInterface* account_store) = 0;

  // Log whether a sync password hash saved.
  virtual void ReportMetrics(const std::string& username,
                             bool is_under_advanced_protection) = 0;

  // Immediately called after |Init()| to retrieve password hash data for
  // reuse detection.
  virtual void PreparePasswordHashData(const std::string& sync_username,
                                       bool is_signed_in) = 0;

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
  // |is_primary_account| is whether account belong to the password is a
  // primary account.
  virtual void SaveGaiaPasswordHash(const std::string& username,
                                    const std::u16string& password,
                                    bool is_primary_account,
                                    GaiaPasswordHashChange event) = 0;

  // Saves |username| and a hash of |password| for enterprise password reuse
  // checking.
  virtual void SaveEnterprisePasswordHash(const std::string& username,
                                          const std::u16string& password) = 0;

  // Saves |sync_password_data| for sync password reuse checking.
  // |event| is used for metric logging.
  virtual void SaveSyncPasswordHash(const PasswordHashData& sync_password_data,
                                    GaiaPasswordHashChange event) = 0;

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
  virtual void SetPasswordStoreSigninNotifier(
      std::unique_ptr<PasswordStoreSigninNotifier> notifier) = 0;

  // Schedules the update of password hashes used by reuse detector.
  // |does_primary_account_exists| and |is_signed_in| fields are only used if
  // |should_log_metrics| is true.
  virtual void SchedulePasswordHashUpdate(bool should_log_metrics,
                                          bool does_primary_account_exists,
                                          bool is_signed_in) = 0;

  // Schedules the update of enterprise login and change password URLs.
  // These URLs are used in enterprise password reuse detection.
  virtual void ScheduleEnterprisePasswordURLUpdate() = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_MANAGER_H_
