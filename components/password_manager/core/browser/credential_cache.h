// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_CACHE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_CACHE_H_

#include <vector>

#include "base/strings/string16.h"
#include "base/util/type_safety/strong_alias.h"
#include "components/password_manager/core/browser/password_form_forward.h"
#include "url/origin.h"

namespace password_manager {

class OriginCredentialStore;

// This class caches and provides credential stores for different origins.
class CredentialCache {
 public:
  // TODO(crbug.com/1051553): Consider reusing this alias for other password
  // manager code as well.
  using IsOriginBlacklisted =
      util::StrongAlias<class IsOriginBlacklistedTag, bool>;
  CredentialCache();
  CredentialCache(const CredentialCache&) = delete;
  CredentialCache& operator=(const CredentialCache&) = delete;
  ~CredentialCache();

  // Saves credentials and blacklisted status for an origin so that they can be
  // used in the sheet.
  void SaveCredentialsAndBlacklistedForOrigin(
      const std::vector<const PasswordForm*>& matches,
      IsOriginBlacklisted is_blacklisted,
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
