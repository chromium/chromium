// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/origin_credential_store.h"

#include <ios>
#include <tuple>
#include <utility>
#include <vector>

#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/password_form.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {

using BlacklistedStatus = OriginCredentialStore::BlacklistedStatus;

UiCredential::UiCredential(base::string16 username,
                           base::string16 password,
                           url::Origin origin,
                           IsPublicSuffixMatch is_public_suffix_match,
                           IsAffiliationBasedMatch is_affiliation_based_match)
    : username_(std::move(username)),
      password_(std::move(password)),
      origin_(std::move(origin)),
      is_public_suffix_match_(is_public_suffix_match),
      is_affiliation_based_match_(is_affiliation_based_match) {}

UiCredential::UiCredential(const PasswordForm& form,
                           const url::Origin& affiliated_origin)
    : username_(form.username_value),
      password_(form.password_value),
      origin_(form.is_affiliation_based_match ? affiliated_origin
                                              : url::Origin::Create(form.url)),
      is_public_suffix_match_(form.is_public_suffix_match),
      is_affiliation_based_match_(form.is_affiliation_based_match) {}

UiCredential::UiCredential(UiCredential&&) = default;
UiCredential::UiCredential(const UiCredential&) = default;
UiCredential& UiCredential::operator=(UiCredential&&) = default;
UiCredential& UiCredential::operator=(const UiCredential&) = default;
UiCredential::~UiCredential() = default;

bool operator==(const UiCredential& lhs, const UiCredential& rhs) {
  auto tie = [](const UiCredential& cred) {
    return std::make_tuple(std::cref(cred.username()),
                           std::cref(cred.password()), std::cref(cred.origin()),
                           cred.is_public_suffix_match(),
                           cred.is_affiliation_based_match());
  };

  return tie(lhs) == tie(rhs);
}

std::ostream& operator<<(std::ostream& os, const UiCredential& credential) {
  return os << "(user: \"" << credential.username() << "\", "
            << "pwd: \"" << credential.password() << "\", "
            << "origin: \"" << credential.origin() << "\", "
            << (credential.is_public_suffix_match() ? "PSL-" : "exact origin ")
            << "match, "
            << "affiliation based match: " << std::boolalpha
            << credential.is_affiliation_based_match();
}

OriginCredentialStore::OriginCredentialStore(url::Origin origin)
    : origin_(std::move(origin)) {}
OriginCredentialStore::~OriginCredentialStore() = default;

void OriginCredentialStore::SaveCredentials(
    std::vector<UiCredential> credentials) {
  credentials_ = std::move(credentials);
}

base::span<const UiCredential> OriginCredentialStore::GetCredentials() const {
  return credentials_;
}

void OriginCredentialStore::SetBlacklistedStatus(bool is_blacklisted) {
  if (is_blacklisted) {
    blacklisted_status_ = BlacklistedStatus::kIsBlacklisted;
    return;
  }

  if (blacklisted_status_ == BlacklistedStatus::kIsBlacklisted) {
    blacklisted_status_ = BlacklistedStatus::kWasBlacklisted;
  }
}

BlacklistedStatus OriginCredentialStore::GetBlacklistedStatus() const {
  return blacklisted_status_;
}

void OriginCredentialStore::ClearCredentials() {
  credentials_.clear();
}

}  // namespace password_manager
