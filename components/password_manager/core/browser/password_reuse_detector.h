// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_DETECTOR_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_DETECTOR_H_

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
#include "components/password_manager/core/browser/password_store/password_store_change.h"

namespace password_manager {

// Minimum number of characters in a password for finding it as password reuse.
// It does not make sense to consider short strings for password reuse, since it
// is quite likely that they are parts of common words.
inline constexpr size_t kMinPasswordLengthToCheck = 4;

class PasswordReuseDetectorConsumer;

// Comparator that compares reversed strings.
struct ReverseStringLess {
  bool operator()(const std::u16string& lhs, const std::u16string& rhs) const;
};

// Container for the signon_realm and username that a compromised saved password
// is saved on/with.
struct MatchingReusedCredential {
  friend auto operator<=>(const MatchingReusedCredential&,
                          const MatchingReusedCredential&) = default;
  friend bool operator==(const MatchingReusedCredential&,
                         const MatchingReusedCredential&) = default;

  std::string signon_realm;
  std::u16string username;
  // The store in which those credentials are stored.
  PasswordForm::Store in_store = PasswordForm::Store::kNotSet;
};

// Per-profile class responsible for detection of password reuse, i.e. that the
// user input on some site contains the password saved on another site.
// It receives saved passwords through PasswordStoreConsumer interface.
// It stores passwords in memory and CheckReuse() can be used for finding
// a password reuse.
class PasswordReuseDetector {
 public:
  PasswordReuseDetector() = default;
  virtual ~PasswordReuseDetector() = default;

  PasswordReuseDetector(const PasswordReuseDetector&) = delete;
  PasswordReuseDetector& operator=(const PasswordReuseDetector&) = delete;

  virtual void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) = 0;

  virtual void OnLoginsChanged(
      const password_manager::PasswordStoreChangeList& changes) = 0;

  virtual void OnLoginsRetained(
      PasswordForm::Store password_store_type,
      const std::vector<PasswordForm>& retained_passwords) = 0;

  // Clears all the cached passwords which are stored on the account store.
  virtual void ClearCachedAccountStorePasswords() = 0;

  // Checks that some suffix of |input| equals to a password saved on another
  // registry controlled domain than |domain| or to a sync password.
  // If such suffix is found, |consumer|->OnReuseFound() is called on the same
  // thread on which this method is called.
  // |consumer| should not be null.
  virtual void CheckReuse(const std::u16string& input,
                          const std::string& domain,
                          PasswordReuseDetectorConsumer* consumer) = 0;

  // Stores a vector of PasswordHashData for Gaia password reuse checking.
  virtual void UseGaiaPasswordHash(
      std::optional<std::vector<PasswordHashData>> password_hash_data_list) = 0;

  // Stores a vector of PasswordHashData for enterprise password reuse checking.
  virtual void UseNonGaiaEnterprisePasswordHash(
      std::optional<std::vector<PasswordHashData>> password_hash_data_list) = 0;

  // Stores enterprise login URLs and change password URL.
  // These URLs should be skipped in enterprise password reuse checking.
  virtual void UseEnterprisePasswordURLs(
      std::optional<std::vector<GURL>> enterprise_login_urls,
      std::optional<GURL> enterprise_change_password_url) = 0;

  virtual void ClearGaiaPasswordHash(const std::string& username) = 0;

  virtual void ClearAllGaiaPasswordHash() = 0;

  virtual void ClearAllEnterprisePasswordHash() = 0;

  virtual void ClearAllNonGmailPasswordHash() = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_DETECTOR_H_
