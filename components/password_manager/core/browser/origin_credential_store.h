// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ORIGIN_CREDENTIAL_STORE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ORIGIN_CREDENTIAL_STORE_H_

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {

struct PasswordForm;

// Encapsulates the data from the password manager backend as used by the UI.
class UiCredential {
 public:
  using IsPublicSuffixMatch =
      base::StrongAlias<class IsPublicSuffixMatchTag, bool>;

  using IsAffiliationBasedMatch =
      base::StrongAlias<class IsAffiliationBasedMatchTag, bool>;

  UiCredential(std::u16string username,
               std::u16string password,
               url::Origin origin,
               IsPublicSuffixMatch is_public_suffix_match,
               IsAffiliationBasedMatch is_affiliation_based_match,
               base::Time last_used);
  UiCredential(const PasswordForm& form, const url::Origin& affiliated_origin);
  UiCredential(UiCredential&&);
  UiCredential(const UiCredential&);
  UiCredential& operator=(UiCredential&&);
  UiCredential& operator=(const UiCredential&);
  ~UiCredential();

  const std::u16string& username() const { return username_; }

  const std::u16string& password() const { return password_; }

  const url::Origin& origin() const { return origin_; }

  IsPublicSuffixMatch is_public_suffix_match() const {
    return is_public_suffix_match_;
  }

  IsAffiliationBasedMatch is_affiliation_based_match() const {
    return is_affiliation_based_match_;
  }

  base::Time last_used() const { return last_used_; }

 private:
  std::u16string username_;
  std::u16string password_;
  url::Origin origin_;
  IsPublicSuffixMatch is_public_suffix_match_{false};
  IsAffiliationBasedMatch is_affiliation_based_match_{false};
  base::Time last_used_;
};

bool operator==(const UiCredential& lhs, const UiCredential& rhs);

std::ostream& operator<<(std::ostream& os, const UiCredential& credential);

// This class stores credential pairs originating from the same origin. The
// store is supposed to be unique per origin per tab. It is designed to share
// credentials without creating unnecessary copies.
class OriginCredentialStore {
 public:
  enum class BlocklistedStatus {
    // The origin was not blocklisted at the moment this store was initialized.
    kNeverBlocklisted,
    // The origin was blocklisted when the store was initialized, but it isn't
    // currently blocklisted.
    kWasBlocklisted,
    // The origin is currently blocklisted.
    kIsBlocklisted
  };

  explicit OriginCredentialStore(url::Origin origin);
  OriginCredentialStore(const OriginCredentialStore&) = delete;
  OriginCredentialStore& operator=(const OriginCredentialStore&) = delete;
  ~OriginCredentialStore();

  // Saves credentials so that they can be used in the UI.
  void SaveCredentials(std::vector<UiCredential> credentials);

  // Returns references to the held credentials (or an empty set if aren't any).
  base::span<const UiCredential> GetCredentials() const;

  // Sets the blocklisted status. The possible transitions are:
  // (*, is_blocklisted = true) -> kIsBlocklisted
  // ((kIsBlocklisted|kWasBlocklisted), is_blocklisted = false)
  //      -> kWasBlocklisted
  // (kNeverBlocklisted, is_blocklisted = false) -> kNeverBlocklisted
  void SetBlocklistedStatus(bool is_blocklisted);

  // Returns the blacklsited status for |origin_|.
  BlocklistedStatus GetBlocklistedStatus() const;

  // Removes all credentials from the store.
  void ClearCredentials();

  // Returns the origin that this store keeps credentials for.
  const url::Origin& origin() const { return origin_; }

 private:
  // Contains all previously stored of credentials.
  std::vector<UiCredential> credentials_;

  // The blocklisted status for |origin_|.
  // Used to know whether unblocklisting UI needs to be displayed and what
  // state it should display;
  BlocklistedStatus blocklisted_status_ = BlocklistedStatus::kNeverBlocklisted;

  // The origin which all stored passwords are related to.
  const url::Origin origin_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ORIGIN_CREDENTIAL_STORE_H_
