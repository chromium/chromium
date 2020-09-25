// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_DETECTOR_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_DETECTOR_H_

#include <stdint.h>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

namespace password_manager {

class PasswordReuseDetectorConsumer;

// Comparator that compares reversed strings.
struct ReverseStringLess {
  bool operator()(const base::string16& lhs, const base::string16& rhs) const;
};

// Container for the signon_realm and username that a compromised saved password
// is saved on/with.
struct MatchingReusedCredential {
  std::string signon_realm;
  base::string16 username;
  // The store in which those credentials are stored.
  PasswordForm::Store in_store = PasswordForm::Store::kNotSet;

  bool operator<(const MatchingReusedCredential& other) const;
  bool operator==(const MatchingReusedCredential& other) const;
};

// Per-profile class responsible for detection of password reuse, i.e. that the
// user input on some site contains the password saved on another site.
// It receives saved passwords through PasswordStoreConsumer interface.
// It stores passwords in memory and CheckReuse() can be used for finding
// a password reuse.
class PasswordReuseDetector : public PasswordStoreConsumer {
 public:
  PasswordReuseDetector();
  ~PasswordReuseDetector() override;

  // PasswordStoreConsumer
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override;

  // Add new or updated passwords from |changes| to internal password index.
  void OnLoginsChanged(const PasswordStoreChangeList& changes);

  // Checks that some suffix of |input| equals to a password saved on another
  // registry controlled domain than |domain| or to a sync password.
  // If such suffix is found, |consumer|->OnReuseFound() is called on the same
  // thread on which this method is called.
  // |consumer| should not be null.
  void CheckReuse(const base::string16& input,
                  const std::string& domain,
                  PasswordReuseDetectorConsumer* consumer);

  // Stores a vector of PasswordHashData for Gaia password reuse checking.
  void UseGaiaPasswordHash(
      base::Optional<std::vector<PasswordHashData>> password_hash_data_list);

  // Stores a vector of PasswordHashData for enterprise password reuse checking.
  void UseNonGaiaEnterprisePasswordHash(
      base::Optional<std::vector<PasswordHashData>> password_hash_data_list);

  // Stores enterprise login URLs and change password URL.
  // These URLs should be skipped in enterprise password reuse checking.
  void UseEnterprisePasswordURLs(
      base::Optional<std::vector<GURL>> enterprise_login_urls,
      base::Optional<GURL> enterprise_change_password_url);

  void ClearGaiaPasswordHash(const std::string& username);

  void ClearAllGaiaPasswordHash();

  void ClearAllEnterprisePasswordHash();

  void ClearAllNonGmailPasswordHash();

 private:
  using PasswordsReusedCredentialsMap =
      std::map<base::string16,
               std::set<MatchingReusedCredential>,
               ReverseStringLess>;

  using passwords_iterator = PasswordsReusedCredentialsMap::const_iterator;

  // Add password from |form| to |passwords_| and
  // |passwords_with_matching_reused_credentials_|.
  void AddPassword(const PasswordForm& form);

  // If Gaia password reuse is found, return the PasswordHashData of the reused
  // password. If no reuse is found, return |base::nullopt|.
  base::Optional<PasswordHashData> CheckGaiaPasswordReuse(
      const base::string16& input,
      const std::string& domain);

  // If Non-Gaia enterprise password reuse is found, return the PasswordHashData
  // of the the reused password. If no reuse is found, return |base::nullopt|.
  base::Optional<PasswordHashData> CheckNonGaiaEnterprisePasswordReuse(
      const base::string16& input,
      const std::string& domain);

  // If saved-password reuse is found, fill in the MatchingReusedCredentials
  // that match any reused password, and return the length of the
  // longest password matched.  If no reuse is found, return 0.
  size_t CheckSavedPasswordReuse(
      const base::string16& input,
      const std::string& domain,
      std::vector<MatchingReusedCredential>* matching_reused_credentials_out);

  // Returns the iterator to |passwords_with_matching_reused_credentials_| that
  // corresponds to the longest key in
  // |passwords_with_matching_reused_credentials_| that is a suffix of |input|.
  // Returns passwords_with_matching_reused_credentials_.end() in case when no
  // key in |passwords_with_matching_reused_credentials_| is a prefix of
  // |input|.
  passwords_iterator FindFirstSavedPassword(const base::string16& input);

  // Call this repeatedly with iterator from |FindFirstSavedPassword| to
  // find other matching passwords. This returns the iterator to
  // |passwords_with_matching_reused_credentials_| that is the next previous
  // matching entry that's a suffix of |input|, or
  // passwords_with_matching_reused_credentials_.end() if there are no more.
  passwords_iterator FindNextSavedPassword(const base::string16& input,
                                           passwords_iterator it);

  // Contains all passwords.
  // A key is a password.
  // A value is a set of pairs of signon_realms and username on which the
  // password is saved.
  // The order of the keys are ordered in lexicographical order of reversed
  // strings. The reason for this is to optimize the lookup time. If the strings
  // were not reversed, it would be needed to loop over the length of the typed
  // input and size of this map and then find the suffix (O(n*m*log(n))).
  // See https://crbug.com/668155.
  PasswordsReusedCredentialsMap passwords_with_matching_reused_credentials_;

  // Number of passwords in |passwords_|, each password is calculated the number
  // of times how many different sites it's saved on.
  int saved_passwords_ = 0;

  base::Optional<std::vector<PasswordHashData>> gaia_password_hash_data_list_;

  base::Optional<std::vector<PasswordHashData>>
      enterprise_password_hash_data_list_;

  base::Optional<std::vector<GURL>> enterprise_password_urls_;

  DISALLOW_COPY_AND_ASSIGN(PasswordReuseDetector);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_DETECTOR_H_
