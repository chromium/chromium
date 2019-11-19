// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_cache.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <utility>
#include <vector>

#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/origin_credential_store.h"

using autofill::PasswordForm;

namespace password_manager {

CredentialCache::CredentialCache() = default;
CredentialCache::~CredentialCache() = default;

void CredentialCache::SaveCredentialsForOrigin(
    const std::vector<const PasswordForm*>& best_matches,
    const url::Origin& origin) {
  std::vector<CredentialPair> credentials;
  credentials.reserve(best_matches.size());
  for (const PasswordForm* form : best_matches) {
    credentials.emplace_back(
        form->username_value, form->password_value, form->origin,
        CredentialPair::IsPublicSuffixMatch(form->is_public_suffix_match));
  }
  // Sort by origin, then username.
  std::sort(credentials.begin(), credentials.end(),
            [](const CredentialPair& lhs, const CredentialPair& rhs) {
              return std::tie(lhs.origin_url, lhs.username) <
                     std::tie(rhs.origin_url, rhs.username);
            });
  // Move credentials with exactly matching origins to the top.
  const GURL url = origin.GetURL();
  std::stable_partition(
      credentials.begin(), credentials.end(),
      [&url](const CredentialPair& pair) { return pair.origin_url == url; });
  GetOrCreateCredentialStore(origin).SaveCredentials(std::move(credentials));
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
