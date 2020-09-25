// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ORIGIN_CREDENTIAL_STORE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ORIGIN_CREDENTIAL_STORE_H_

#include <vector>

#include "base/containers/span.h"
#include "base/strings/string16.h"
#include "base/util/type_safety/strong_alias.h"
#include "components/password_manager/core/browser/password_form_forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {

// Encapsulates the data from the password manager backend as used by the UI.
class UiCredential {
 public:
  using IsPublicSuffixMatch =
      util::StrongAlias<class IsPublicSuffixMatchTag, bool>;

  using IsAffiliationBasedMatch =
      util::StrongAlias<class IsAffiliationBasedMatchTag, bool>;

  UiCredential(base::string16 username,
               base::string16 password,
               url::Origin origin,
               IsPublicSuffixMatch is_public_suffix_match,
               IsAffiliationBasedMatch is_affiliation_based_match);
  UiCredential(const PasswordForm& form, const url::Origin& affiliated_origin);
  UiCredential(UiCredential&&);
  UiCredential(const UiCredential&);
  UiCredential& operator=(UiCredential&&);
  UiCredential& operator=(const UiCredential&);
  ~UiCredential();

  const base::string16& username() const { return username_; }

  const base::string16& password() const { return password_; }

  const url::Origin& origin() const { return origin_; }

  IsPublicSuffixMatch is_public_suffix_match() const {
    return is_public_suffix_match_;
  }

  IsAffiliationBasedMatch is_affiliation_based_match() const {
    return is_affiliation_based_match_;
  }

 private:
  base::string16 username_;
  base::string16 password_;
  url::Origin origin_;
  IsPublicSuffixMatch is_public_suffix_match_{false};
  IsAffiliationBasedMatch is_affiliation_based_match_{false};
};

bool operator==(const UiCredential& lhs, const UiCredential& rhs);

std::ostream& operator<<(std::ostream& os, const UiCredential& credential);

// This class stores credential pairs originating from the same origin. The
// store is supposed to be unique per origin per tab. It is designed to share
// credentials without creating unnecessary copies.
class OriginCredentialStore {
 public:
  enum class BlacklistedStatus {
    // The origin was not blacklisted at the moment this store was initialized.
    kNeverBlacklisted,
    // The origin was blacklisted when the store was initialized, but it isn't
    // currently blacklisted.
    kWasBlacklisted,
    // The origin is currently blacklisted.
    kIsBlacklisted
  };

  explicit OriginCredentialStore(url::Origin origin);
  OriginCredentialStore(const OriginCredentialStore&) = delete;
  OriginCredentialStore& operator=(const OriginCredentialStore&) = delete;
  ~OriginCredentialStore();

  // Saves credentials so that they can be used in the UI.
  void SaveCredentials(std::vector<UiCredential> credentials);

  // Returns references to the held credentials (or an empty set if aren't any).
  base::span<const UiCredential> GetCredentials() const;

  // Sets the blacklisted status. The possible transitions are:
  // (*, is_blacklisted = true) -> kIsBlacklisted
  // ((kIsBlacklisted|kWasBlacklisted), is_blacklisted = false)
  //      -> kWasBlacklisted
  // (kNeverBlacklisted, is_blacklisted = false) -> kNeverBlacklisted
  void SetBlacklistedStatus(bool is_blacklisted);

  // Returns the blacklsited status for |origin_|.
  BlacklistedStatus GetBlacklistedStatus() const;

  // Removes all credentials from the store.
  void ClearCredentials();

  // Returns the origin that this store keeps credentials for.
  const url::Origin& origin() const { return origin_; }

 private:
  // Contains all previously stored of credentials.
  std::vector<UiCredential> credentials_;

  // The blacklisted status for |origin_|.
  // Used to know whether unblacklisting UI needs to be displayed and what
  // state it should display;
  BlacklistedStatus blacklisted_status_ = BlacklistedStatus::kNeverBlacklisted;

  // The origin which all stored passwords are related to.
  const url::Origin origin_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ORIGIN_CREDENTIAL_STORE_H_
