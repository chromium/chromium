// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/origin_credential_store.h"

#include <utility>
#include <vector>

#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"

namespace password_manager {
namespace {

GURL GetAndroidOrOriginURL(const GURL& url) {
  if (IsValidAndroidFacetURI(url.spec()))
    return url;  // Pass android origins as they are.
  return url.GetOrigin();
}

}  // namespace

CredentialPair::CredentialPair(base::string16 username,
                               base::string16 password,
                               const GURL& origin_url,
                               IsPublicSuffixMatch is_public_suffix_match)
    : username(std::move(username)),
      password(std::move(password)),
      origin_url(GetAndroidOrOriginURL(origin_url)),
      is_public_suffix_match(is_public_suffix_match) {}
CredentialPair::CredentialPair(CredentialPair&&) = default;
CredentialPair::CredentialPair(const CredentialPair&) = default;
CredentialPair& CredentialPair::operator=(CredentialPair&&) = default;
CredentialPair& CredentialPair::operator=(const CredentialPair&) = default;
CredentialPair::~CredentialPair() = default;

bool operator==(const CredentialPair& lhs, const CredentialPair& rhs) {
  return lhs.username == rhs.username && lhs.password == rhs.password &&
         lhs.origin_url == rhs.origin_url &&
         lhs.is_public_suffix_match == rhs.is_public_suffix_match;
}

std::ostream& operator<<(std::ostream& os, const CredentialPair& pair) {
  os << "(user: \"" << pair.username << "\", "
     << "pwd: \"" << pair.password << "\", "
     << "origin: \"" << pair.origin_url << "\", "
     << (pair.is_public_suffix_match ? "PSL-" : "exact origin ") << "match)";
  return os;
}

OriginCredentialStore::OriginCredentialStore(url::Origin origin)
    : origin_(std::move(origin)) {}
OriginCredentialStore::~OriginCredentialStore() = default;

void OriginCredentialStore::SaveCredentials(
    std::vector<CredentialPair> credentials) {
  credentials_ = std::move(credentials);
}

base::span<const CredentialPair> OriginCredentialStore::GetCredentials() const {
  return credentials_;
}

void OriginCredentialStore::ClearCredentials() {
  credentials_.clear();
}

}  // namespace password_manager
