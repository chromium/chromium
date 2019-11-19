// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ORIGIN_CREDENTIAL_STORE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ORIGIN_CREDENTIAL_STORE_H_

#include <vector>

#include "base/containers/span.h"
#include "base/strings/string16.h"
#include "base/util/type_safety/strong_alias.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {

// Encapsulates the data from the password manager backend as used by the UI.
struct CredentialPair {
  using IsPublicSuffixMatch =
      util::StrongAlias<class IsPublicSuffixMatchTag, bool>;

  CredentialPair(base::string16 username,
                 base::string16 password,
                 const GURL& origin_url,
                 IsPublicSuffixMatch is_public_suffix_match);
  CredentialPair(CredentialPair&&);
  CredentialPair(const CredentialPair&);
  CredentialPair& operator=(CredentialPair&&);
  CredentialPair& operator=(const CredentialPair&);
  ~CredentialPair();

  base::string16 username;
  base::string16 password;
  GURL origin_url;  // Could be android:// which url::Origin doesn't support.
  IsPublicSuffixMatch is_public_suffix_match{false};
};

bool operator==(const CredentialPair& lhs, const CredentialPair& rhs);

std::ostream& operator<<(std::ostream& os, const CredentialPair& pair);

// This class stores credential pairs originating from the same origin. The
// store is supposed to be unique per origin per tab. It is designed to share
// credentials without creating unnecessary copies.
class OriginCredentialStore {
 public:
  explicit OriginCredentialStore(url::Origin origin);
  OriginCredentialStore(const OriginCredentialStore&) = delete;
  OriginCredentialStore& operator=(const OriginCredentialStore&) = delete;
  ~OriginCredentialStore();

  // Saves credentials so that they can be used in the UI.
  void SaveCredentials(std::vector<CredentialPair> credentials);

  // Returns references to the held credentials (or an empty set if aren't any).
  base::span<const CredentialPair> GetCredentials() const;

  // Removes all credentials from the store.
  void ClearCredentials();

  // Returns the origin that this store keeps credentials for.
  const url::Origin& origin() const { return origin_; }

 private:
  // Contains all previously stored of credentials.
  std::vector<CredentialPair> credentials_;

  // The origin which all stored passwords are related to.
  const url::Origin origin_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ORIGIN_CREDENTIAL_STORE_H_
