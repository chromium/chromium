// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_MANAGER_H_

#include <memory>

#include "base/sequenced_task_runner.h"
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/password_manager/core/browser/password_reuse_detector_consumer.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

class PrefService;

namespace password_manager {

class PasswordStoreSigninNotifier;

using PasswordHashDataList = absl::optional<std::vector<PasswordHashData>>;
using metrics_util::GaiaPasswordHashChange;

// Per-store class responsible for detection of password reuse, i.e. that the
// user input on some site contains the password saved on another site.
class PasswordReuseManager : public PasswordStoreConsumer {
 public:
  PasswordReuseManager();
  ~PasswordReuseManager() override;

  PasswordReuseManager(const PasswordReuseManager&) = delete;
  PasswordReuseManager& operator=(const PasswordReuseManager&) = delete;

  // Always call this on the UI thread.
  void Init(PrefService* prefs, PasswordStoreInterface* store);

  // Log whether a sync password hash saved.
  void ReportMetrics(const std::string& username,
                     bool is_under_advanced_protection);

  // Immediately called after |Init()| to retrieve password hash data for
  // reuse detection.
  void PreparePasswordHashData(const std::string& sync_username,
                               bool is_signed_in);

  // Checks that some suffix of |input| equals to a password saved on another
  // registry controlled domain than |domain|.
  // If such suffix is found, |consumer|->OnReuseFound() is called on the main
  // sequence.
  // |consumer| must not be null.
  void CheckReuse(const std::u16string& input,
                  const std::string& domain,
                  PasswordReuseDetectorConsumer* consumer);

  // Saves |username| and a hash of |password| for GAIA password reuse checking.
  // |event| is used for metric logging and for distinguishing sync password
  // hash change event and other non-sync GAIA password change event.
  // |is_primary_account| is whether account belong to the password is a
  // primary account.
  void SaveGaiaPasswordHash(const std::string& username,
                            const std::u16string& password,
                            bool is_primary_account,
                            GaiaPasswordHashChange event);

  // Saves |username| and a hash of |password| for enterprise password reuse
  // checking.
  void SaveEnterprisePasswordHash(const std::string& username,
                                  const std::u16string& password);

  // Saves |sync_password_data| for sync password reuse checking.
  // |event| is used for metric logging.
  void SaveSyncPasswordHash(const PasswordHashData& sync_password_data,
                            GaiaPasswordHashChange event);

  // Clears the saved GAIA password hash for |username|.
  // TODO(crbug.bom/715987): Remove virtual and make and abstract base class.
  virtual void ClearGaiaPasswordHash(const std::string& username);

  // Clears all the GAIA password hash.
  // TODO(crbug.bom/715987): Remove virtual and make and abstract base class.
  virtual void ClearAllGaiaPasswordHash();

  // Clears all (non-GAIA) enterprise password hash.
  void ClearAllEnterprisePasswordHash();

  // Clear all GAIA password hash that is not associated with a Gmail account.
  void ClearAllNonGmailPasswordHash();

  // Adds a listener on |hash_password_manager_| for when |kHashPasswordData|
  // list might have changed. Should only be called on the UI thread.
  base::CallbackListSubscription RegisterStateCallbackOnHashPasswordManager(
      const base::RepeatingCallback<void(const std::string& username)>&
          callback);

  // Shouldn't be called more than once, |notifier| must be not nullptr.
  void SetPasswordStoreSigninNotifier(
      std::unique_ptr<PasswordStoreSigninNotifier> notifier);

  // Schedules the update of password hashes used by reuse detector.
  // |does_primary_account_exists| and |is_signed_in| fields are only used if
  // |should_log_metrics| is true.
  void SchedulePasswordHashUpdate(bool should_log_metrics,
                                  bool does_primary_account_exists,
                                  bool is_signed_in);

  // Schedules the update of enterprise login and change password URLs.
  // These URLs are used in enterprise password reuse detection.
  void ScheduleEnterprisePasswordURLUpdate();

 private:
  // PasswordStoreConsumer.
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override;

  // Saves |username| and a hash of |password| for password reuse checking.
  // |is_gaia_password| indicates if it is a Gaia account. |event| is used for
  // metric logging. |is_primary_account| is whether account belong to the
  // password is a primary account.
  void SaveProtectedPasswordHash(const std::string& username,
                                 const std::u16string& password,
                                 bool is_primary_account,
                                 bool is_gaia_password,
                                 GaiaPasswordHashChange event);

  // Schedules the given |task| to be run on the 'background_task_runner_'.
  bool ScheduleTask(base::OnceClosure task);

  // TaskRunner for tasks that run on the main sequence (the UI thread).
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  // TaskRunner for all the background operations.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  PrefService* prefs_ = nullptr;

  // The 'reuse_detector_', owned by this PasswordReuseManager instance, but
  // living on the background thread. It will be deleted asynchronously during
  // shutdown on the background thread, so it will outlive |this| along with all
  // its in-flight tasks.
  PasswordReuseDetector* reuse_detector_ = nullptr;

  // Notifies PasswordReuseManager about sign-in events.
  std::unique_ptr<PasswordStoreSigninNotifier> notifier_;

  // Responsible for saving, clearing, retrieving and encryption of a password
  // hash data in preferences.
  HashPasswordManager hash_password_manager_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_MANAGER_H_
