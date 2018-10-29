// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_DEFAULT_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_DEFAULT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_store.h"

namespace password_manager {

// Simple password store implementation that delegates everything to
// the LoginDatabase.
class PasswordStoreDefault : public PasswordStore {
 public:
  // The |login_db| must not have been Init()-ed yet. It will be initialized in
  // a deferred manner on the background sequence.
  explicit PasswordStoreDefault(std::unique_ptr<LoginDatabase> login_db);

  void ShutdownOnUIThread() override;

#if defined(USE_X11)
  // Dispose the current |login_db_| and use |login_db|. |login_db| is expected
  // to have been initialised. A null value is equivalent to a database which
  // can't be opened.
  // TODO(crbug.com/571003) This is only used to migrate Linux to an encrypted
  // LoginDatabase.
  void SetLoginDB(std::unique_ptr<LoginDatabase> login_db);
#endif  // defined(USE_X11)

  // To be used only for testing or in subclasses.
  LoginDatabase* login_db() const { return login_db_.get(); }

 protected:
  ~PasswordStoreDefault() override;

  // Opens |login_db_| on the background sequence.
  bool InitOnBackgroundSequence(
      const syncer::SyncableService::StartSyncFlare& flare) override;

  // Implements PasswordStore interface.
  void ReportMetricsImpl(const std::string& sync_username,
                         bool custom_passphrase_sync_enabled) override;
  PasswordStoreChangeList AddLoginImpl(
      const autofill::PasswordForm& form) override;
  PasswordStoreChangeList UpdateLoginImpl(
      const autofill::PasswordForm& form) override;
  PasswordStoreChangeList RemoveLoginImpl(
      const autofill::PasswordForm& form) override;
  PasswordStoreChangeList RemoveLoginsByURLAndTimeImpl(
      const base::Callback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end) override;
  PasswordStoreChangeList RemoveLoginsCreatedBetweenImpl(
      base::Time delete_begin,
      base::Time delete_end) override;
  PasswordStoreChangeList RemoveLoginsSyncedBetweenImpl(
      base::Time delete_begin,
      base::Time delete_end) override;
  PasswordStoreChangeList DisableAutoSignInForOriginsImpl(
      const base::Callback<bool(const GURL&)>& origin_filter) override;
  bool RemoveStatisticsByOriginAndTimeImpl(
      const base::Callback<bool(const GURL&)>& origin_filter,
      base::Time delete_begin,
      base::Time delete_end) override;
  std::vector<std::unique_ptr<autofill::PasswordForm>> FillMatchingLogins(
      const FormDigest& form) override;
  std::vector<std::unique_ptr<autofill::PasswordForm>>
  FillLoginsForSameOrganizationName(const std::string& signon_realm) override;
  bool FillAutofillableLogins(
      std::vector<std::unique_ptr<autofill::PasswordForm>>* forms) override;
  bool FillBlacklistLogins(
      std::vector<std::unique_ptr<autofill::PasswordForm>>* forms) override;
  DatabaseCleanupResult DeleteUndecryptableLogins() override;
  void AddSiteStatsImpl(const InteractionsStats& stats) override;
  void RemoveSiteStatsImpl(const GURL& origin_domain) override;
  std::vector<InteractionsStats> GetAllSiteStatsImpl() override;
  std::vector<InteractionsStats> GetSiteStatsImpl(
      const GURL& origin_domain) override;

  inline bool DeleteAndRecreateDatabaseFile() {
    return login_db_->DeleteAndRecreateDatabaseFile();
  }

 private:
  // Resets |login_db_| on the background sequence.
  void ResetLoginDB();

  // The login SQL database. The LoginDatabase instance is received via the
  // in an uninitialized state, so as to allow injecting mocks, then Init() is
  // called on the background sequence in a deferred manner. If opening the DB
  // fails, |login_db_| will be reset and stay NULL for the lifetime of |this|.
  std::unique_ptr<LoginDatabase> login_db_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreDefault);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_DEFAULT_H_
