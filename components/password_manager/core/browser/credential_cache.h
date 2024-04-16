// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_CACHE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_CACHE_H_

#include <map>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/types/strong_alias.h"
#include "url/origin.h"

namespace password_manager {

class OriginCredentialStore;
struct PasswordForm;

// This class caches and provides credential stores for different origins.
class CredentialCache {
 public:
  // TODO(crbug.com/40673832): Consider reusing this alias for other password
  // manager code as well.
  using IsOriginBlocklisted =
      base::StrongAlias<class IsOriginBlocklistedTag, bool>;
  CredentialCache();
  CredentialCache(const CredentialCache&) = delete;
  CredentialCache& operator=(const CredentialCache&) = delete;
  ~CredentialCache();

  // Saves credentials and blocklisted status for an origin so that they can be
  // used in the sheet.
  void SaveCredentialsAndBlocklistedForOrigin(
      base::span<const PasswordForm> matches,
      IsOriginBlocklisted is_blocklisted,
      const url::Origin& origin);

  // Returns the credential store for a given origin. If it does not exist, an
  // empty store will be created.
  const OriginCredentialStore& GetCredentialStore(const url::Origin& origin);

  // Removes all credentials for all origins.
  void ClearCredentials();

 private:
  OriginCredentialStore& GetOrCreateCredentialStore(const url::Origin& origin);

  // Contains the store for credential of each requested origin.
  std::map<url::Origin, OriginCredentialStore> origin_credentials_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_CACHE_H_
