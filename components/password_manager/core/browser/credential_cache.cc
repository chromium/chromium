// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_cache.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/password_form.h"
#include "url/origin.h"

namespace password_manager {

CredentialCache::CredentialCache() = default;
CredentialCache::~CredentialCache() = default;

void CredentialCache::SaveCredentialsAndBlocklistedForOrigin(
    base::span<const PasswordForm> best_matches,
    IsOriginBlocklisted is_blocklisted,
    const url::Origin& origin) {
  std::vector<UiCredential> credentials;
  credentials.reserve(best_matches.size());
  for (const PasswordForm& form : best_matches) {
    credentials.emplace_back(form, origin);
  }

  // Sort by origin, then username.
  std::sort(credentials.begin(), credentials.end(),
            [](const UiCredential& lhs, const UiCredential& rhs) {
              return std::tie(lhs.origin(), lhs.username()) <
                     std::tie(rhs.origin(), rhs.username());
            });
  // Move credentials with exactly matching origins to the top.
  std::stable_partition(credentials.begin(), credentials.end(),
                        [&origin](const UiCredential& credential) {
                          return credential.origin() == origin;
                        });
  // Move unnotified shared credentials to the top.
  auto is_unnotified_shared_credential = [](const UiCredential& credential) {
    return credential.is_shared() &&
           !credential.sharing_notification_displayed();
  };
  std::stable_partition(credentials.begin(), credentials.end(),
                        is_unnotified_shared_credential);

  GetOrCreateCredentialStore(origin).SaveCredentials(std::move(credentials));

  GetOrCreateCredentialStore(origin).SetBlocklistedStatus(
      is_blocklisted.value());

  std::vector<PasswordForm> unnotified_shared_credentials;
  for (const PasswordForm& form : best_matches) {
    if (form.type == PasswordForm::Type::kReceivedViaSharing &&
        !form.sharing_notification_displayed) {
      // The cache is only useful when the sharing notification UI is displayed
      // since it is used to mark those credentials as notified after the user
      // interacts with the UI.
        unnotified_shared_credentials.push_back(form);
    }
  }
  GetOrCreateCredentialStore(origin).SaveUnnotifiedSharedCredentials(
      std::move(unnotified_shared_credentials));
}

const OriginCredentialStore& CredentialCache::GetCredentialStore(
    const url::Origin& origin) {
  return GetOrCreateCredentialStore(origin);
}

void CredentialCache::ClearCredentials() {
  origin_credentials_.clear();
}

OriginCredentialStore& CredentialCache::GetOrCreateCredentialStore(
    const url::Origin& origin) {
  return origin_credentials_.emplace(origin, origin).first->second;
}

}  // namespace password_manager
