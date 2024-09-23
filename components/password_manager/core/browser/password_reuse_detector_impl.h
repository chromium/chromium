// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_DETECTOR_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_DETECTOR_IMPL_H_

#include <stdint.h>

#include <compare>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/password_manager/core/browser/password_store/password_store_change.h"

namespace password_manager {

class PasswordReuseDetectorConsumer;

// Per-profile class responsible for detection of password reuse, i.e. that the
// user input on some site contains the password saved on another site.
// It receives saved passwords through PasswordStoreConsumer interface.
// It stores passwords in memory and CheckReuse() can be used for finding
// a password reuse.
class PasswordReuseDetectorImpl : public PasswordReuseDetector {
 public:
  PasswordReuseDetectorImpl();
  ~PasswordReuseDetectorImpl() override;

  PasswordReuseDetectorImpl(const PasswordReuseDetectorImpl&) = delete;
  PasswordReuseDetectorImpl& operator=(const PasswordReuseDetectorImpl&) =
      delete;

  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override;

  void OnLoginsChanged(
      const password_manager::PasswordStoreChangeList& changes) override;

  void OnLoginsRetained(
      PasswordForm::Store password_store_type,
      const std::vector<PasswordForm>& retained_passwords) override;

  // Clears all the cached passwords which are stored on the account store.
  void ClearCachedAccountStorePasswords() override;

  // Checks that some suffix of |input| equals to a password saved on another
  // registry controlled domain than |domain| or to a sync password.
  // If such suffix is found, |consumer|->OnReuseFound() is called on the same
  // thread on which this method is called.
  // |consumer| should not be null.
  void CheckReuse(const std::u16string& input,
                  const std::string& domain,
                  PasswordReuseDetectorConsumer* consumer) override;

  // Stores a vector of PasswordHashData for Gaia password reuse checking.
  void UseGaiaPasswordHash(std::optional<std::vector<PasswordHashData>>
                               password_hash_data_list) override;

  // Stores a vector of PasswordHashData for enterprise password reuse checking.
  void UseNonGaiaEnterprisePasswordHash(
      std::optional<std::vector<PasswordHashData>> password_hash_data_list)
      override;

  // Stores enterprise login URLs and change password URL.
  // These URLs should be skipped in enterprise password reuse checking.
  void UseEnterprisePasswordURLs(
      std::optional<std::vector<GURL>> enterprise_login_urls,
      std::optional<GURL> enterprise_change_password_url) override;

  void ClearGaiaPasswordHash(const std::string& username) override;

  void ClearAllGaiaPasswordHash() override;

  void ClearAllEnterprisePasswordHash() override;

  void ClearAllNonGmailPasswordHash() override;

 private:
  using PasswordsReusedCredentialsMap =
      std::map<std::u16string,
               std::set<MatchingReusedCredential>,
               ReverseStringLess>;

  using passwords_iterator = PasswordsReusedCredentialsMap::const_iterator;

  // Add password from |form| to |passwords_| and
  // |passwords_with_matching_reused_credentials_|.
  void AddPassword(const PasswordForm& form);

  // Remove password of |form| from
  // |passwords_with_matching_reused_credentials_|;
  void RemovePassword(const PasswordForm& form);

  // Removes all the credentials coming from |store_type| store from
  // |passwords_with_matching_reused_credentials_|.
  void RemoveAllLoginsByStoreType(PasswordForm::Store store_type);

  // If Gaia password reuse is found, return the PasswordHashData of the reused
  // password. If no reuse is found, return |std::nullopt|.
  std::optional<PasswordHashData> CheckGaiaPasswordReuse(
      const std::u16string& input,
      const std::string& domain);

  // If Non-Gaia enterprise password reuse is found, return the PasswordHashData
  // of the the reused password. If no reuse is found, return |std::nullopt|.
  std::optional<PasswordHashData> CheckNonGaiaEnterprisePasswordReuse(
      const std::u16string& input,
      const std::string& domain);

  // If saved-password reuse is found, fill in the MatchingReusedCredentials
  // that match any reused password, and return the longest password matched. If
  // no reuse is found, return an empty string.
  std::u16string CheckSavedPasswordReuse(
      const std::u16string& input,
      const std::string& domain,
      std::vector<MatchingReusedCredential>* matching_reused_credentials_out);

  // Returns the iterator to |passwords_with_matching_reused_credentials_| that
  // corresponds to the longest key in
  // |passwords_with_matching_reused_credentials_| that is a suffix of |input|.
  // Returns passwords_with_matching_reused_credentials_.end() in case when no
  // key in |passwords_with_matching_reused_credentials_| is a prefix of
  // |input|.
  passwords_iterator FindFirstSavedPassword(const std::u16string& input);

  // Call this repeatedly with iterator from |FindFirstSavedPassword| to
  // find other matching passwords. This returns the iterator to
  // |passwords_with_matching_reused_credentials_| that is the next previous
  // matching entry that's a suffix of |input|, or
  // passwords_with_matching_reused_credentials_.end() if there are no more.
  passwords_iterator FindNextSavedPassword(const std::u16string& input,
                                           passwords_iterator it);

  // Number of passwords in |passwords_with_matching_reused_credentials_|,
  // each password is calculated the number of times how many different sites
  // it's saved on.
  size_t SavedPasswordsCount();

  // Ensures that all methods, excluding construction, are called on the same
  // sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  // Contains all passwords.
  // A key is a password.
  // A value is a set of pairs of signon_realms and username on which the
  // password is saved.
  // The order of the keys are ordered in lexicographical order of reversed
  // strings. The reason for this is to optimize the lookup time. If the strings
  // were not reversed, it would be needed to loop over the length of the typed
  // input and size of this map and then find the suffix (O(n*m*log(n))).
  // See https://crbug.com/668155.
  PasswordsReusedCredentialsMap passwords_with_matching_reused_credentials_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::optional<std::vector<PasswordHashData>> gaia_password_hash_data_list_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::optional<std::vector<PasswordHashData>>
      enterprise_password_hash_data_list_ GUARDED_BY_CONTEXT(sequence_checker_);

  std::optional<std::vector<GURL>> enterprise_password_urls_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_DETECTOR_IMPL_H_
