// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_reuse_detector.h"

#include <algorithm>
#include <utility>

#include "base/stl_util.h"
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_hash_data.h"
#include "components/password_manager/core/browser/password_reuse_detector_consumer.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "url/origin.h"

using url::Origin;

namespace password_manager {

namespace {
// Minimum number of characters in a password for finding it as password reuse.
// It does not make sense to consider short strings for password reuse, since it
// is quite likely that they are parts of common words.
constexpr size_t kMinPasswordLengthToCheck = 8;

// Returns true iff |suffix_candidate| is a suffix of |str|.
bool IsSuffix(const base::string16& str,
              const base::string16& suffix_candidate) {
  if (str.size() < suffix_candidate.size())
    return false;
  return std::equal(suffix_candidate.rbegin(), suffix_candidate.rend(),
                    str.rbegin());
}

// Helper function to returns matching PasswordHashData from a list that has
// the longest password length.
base::Optional<PasswordHashData> FindPasswordReuse(
    const base::string16& input,
    const std::vector<PasswordHashData>& password_hash_list) {
  base::Optional<PasswordHashData> longest_match = base::nullopt;
  size_t longest_match_size = 0;
  for (const PasswordHashData& hash_data : password_hash_list) {
    if (input.size() < hash_data.length)
      continue;
    size_t offset = input.size() - hash_data.length;
    base::string16 reuse_candidate = input.substr(offset);
    // It is possible that input matches multiple passwords in the list,
    // we only return the first match due to simplicity.
    if (CalculatePasswordHash(reuse_candidate, hash_data.salt) ==
            hash_data.hash &&
        hash_data.length > longest_match_size) {
      longest_match_size = hash_data.length;
      longest_match = hash_data;
    }
  }
  return longest_match;
}

}  // namespace

bool ReverseStringLess::operator()(const base::string16& lhs,
                                   const base::string16& rhs) const {
  return std::lexicographical_compare(lhs.rbegin(), lhs.rend(), rhs.rbegin(),
                                      rhs.rend());
}

bool MatchingReusedCredential::operator<(
    const MatchingReusedCredential& other) const {
  return std::tie(signon_realm, username, in_store) <
         std::tie(other.signon_realm, other.username, other.in_store);
}

bool MatchingReusedCredential::operator==(
    const MatchingReusedCredential& other) const {
  return signon_realm == other.signon_realm && username == other.username &&
         in_store == other.in_store;
}

PasswordReuseDetector::PasswordReuseDetector() = default;

PasswordReuseDetector::~PasswordReuseDetector() = default;

void PasswordReuseDetector::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  for (const auto& form : results)
    AddPassword(*form);
}

void PasswordReuseDetector::OnLoginsChanged(
    const PasswordStoreChangeList& changes) {
  for (const auto& change : changes) {
    if (change.type() == PasswordStoreChange::ADD ||
        change.type() == PasswordStoreChange::UPDATE)
      AddPassword(change.form());
  }
}

void PasswordReuseDetector::CheckReuse(
    const base::string16& input,
    const std::string& domain,
    PasswordReuseDetectorConsumer* consumer) {
  DCHECK(consumer);
  if (input.size() < kMinPasswordLengthToCheck) {
    consumer->OnReuseCheckDone(false, 0, base::nullopt, {}, saved_passwords_);
    return;
  }

  base::Optional<PasswordHashData> reused_gaia_password_hash =
      CheckGaiaPasswordReuse(input, domain);
  size_t gaia_reused_password_length = reused_gaia_password_hash.has_value()
                                           ? reused_gaia_password_hash->length
                                           : 0;

  base::Optional<PasswordHashData> reused_enterprise_password_hash =
      CheckNonGaiaEnterprisePasswordReuse(input, domain);
  size_t enterprise_reused_password_length =
      reused_enterprise_password_hash.has_value()
          ? reused_enterprise_password_hash->length
          : 0;

  std::vector<MatchingReusedCredential> matching_reused_credentials;
  size_t saved_reused_password_length =
      CheckSavedPasswordReuse(input, domain, &matching_reused_credentials);

  size_t max_reused_password_length =
      std::max({saved_reused_password_length, gaia_reused_password_length,
                enterprise_reused_password_length});

  if (max_reused_password_length == 0) {
    consumer->OnReuseCheckDone(false, 0, base::nullopt, {}, saved_passwords_);
    return;
  }

  base::Optional<PasswordHashData> reused_protected_password_hash =
      base::nullopt;
  if (gaia_reused_password_length > enterprise_reused_password_length) {
    reused_protected_password_hash = std::move(reused_gaia_password_hash);
  } else if (enterprise_reused_password_length != 0) {
    reused_protected_password_hash = std::move(reused_enterprise_password_hash);
  }
  consumer->OnReuseCheckDone(true, max_reused_password_length,
                             reused_protected_password_hash,
                             matching_reused_credentials, saved_passwords_);
}

base::Optional<PasswordHashData> PasswordReuseDetector::CheckGaiaPasswordReuse(
    const base::string16& input,
    const std::string& domain) {
  if (!gaia_password_hash_data_list_.has_value() ||
      gaia_password_hash_data_list_->empty()) {
    return base::nullopt;
  }

  // Skips password reuse check if |domain| matches Gaia origin.
  const Origin gaia_origin =
      Origin::Create(GaiaUrls::GetInstance()->gaia_url().GetOrigin());
  if (Origin::Create(GURL(domain)).IsSameOriginWith(gaia_origin))
    return base::nullopt;

  return FindPasswordReuse(input, gaia_password_hash_data_list_.value());
}

base::Optional<PasswordHashData>
PasswordReuseDetector::CheckNonGaiaEnterprisePasswordReuse(
    const base::string16& input,
    const std::string& domain) {
  if (!enterprise_password_hash_data_list_.has_value() ||
      enterprise_password_hash_data_list_->empty()) {
    return base::nullopt;
  }

  // Skips password reuse check if |domain| matches enterprise login URL or
  // enterprise change password URL.
  GURL page_url(domain);
  if (enterprise_password_urls_.has_value() &&
      safe_browsing::MatchesURLList(page_url,
                                    enterprise_password_urls_.value())) {
    return base::nullopt;
  }

  return FindPasswordReuse(input, enterprise_password_hash_data_list_.value());
}

size_t PasswordReuseDetector::CheckSavedPasswordReuse(
    const base::string16& input,
    const std::string& domain,
    std::vector<MatchingReusedCredential>* matching_reused_credentials_out) {
  const std::string registry_controlled_domain =
      GetRegistryControlledDomain(GURL(domain));

  // More than one password call match |input| if they share a common suffix
  // with |input|.  Collect the set of MatchingReusedCredential for all matches.
  std::set<MatchingReusedCredential> matching_reused_credentials_set;

  // The longest password match is kept for metrics.
  size_t longest_match_len = 0;

  for (auto passwords_iterator = FindFirstSavedPassword(input);
       passwords_iterator != passwords_with_matching_reused_credentials_.end();
       passwords_iterator = FindNextSavedPassword(input, passwords_iterator)) {
    const std::set<MatchingReusedCredential>& credentials =
        passwords_iterator->second;
    DCHECK(!credentials.empty());

    std::set<std::string> signon_realms;
    for (const auto& credential : credentials) {
      signon_realms.insert(
          GetRegistryControlledDomain(GURL(credential.signon_realm)));
    }
    // If the page's URL matches a saved domain for this password,
    // this isn't password-reuse.
    if (base::Contains(signon_realms, registry_controlled_domain))
      continue;

    matching_reused_credentials_set.insert(credentials.begin(),
                                           credentials.end());
    DCHECK(!passwords_iterator->first.empty());
    longest_match_len =
        std::max(longest_match_len, passwords_iterator->first.size());
  }

  matching_reused_credentials_out->assign(
      matching_reused_credentials_set.begin(),
      matching_reused_credentials_set.end());

  return longest_match_len;
}

void PasswordReuseDetector::UseGaiaPasswordHash(
    base::Optional<std::vector<PasswordHashData>> password_hash_data_list) {
  gaia_password_hash_data_list_ = std::move(password_hash_data_list);
}

void PasswordReuseDetector::UseNonGaiaEnterprisePasswordHash(
    base::Optional<std::vector<PasswordHashData>> password_hash_data_list) {
  enterprise_password_hash_data_list_ = std::move(password_hash_data_list);
}

void PasswordReuseDetector::UseEnterprisePasswordURLs(
    base::Optional<std::vector<GURL>> enterprise_login_urls,
    base::Optional<GURL> enterprise_change_password_url) {
  enterprise_password_urls_ = std::move(enterprise_login_urls);
  if (!enterprise_change_password_url.has_value() ||
      !enterprise_change_password_url->is_valid()) {
    return;
  }

  if (!enterprise_password_urls_)
    enterprise_password_urls_ = base::make_optional<std::vector<GURL>>();
  enterprise_password_urls_->push_back(enterprise_change_password_url.value());
}

void PasswordReuseDetector::ClearGaiaPasswordHash(const std::string& username) {
  if (!gaia_password_hash_data_list_)
    return;

  base::EraseIf(*gaia_password_hash_data_list_,
                [&username](const PasswordHashData& data) {
                  return data.username == username;
                });
}

void PasswordReuseDetector::ClearAllGaiaPasswordHash() {
  gaia_password_hash_data_list_.reset();
}

void PasswordReuseDetector::ClearAllEnterprisePasswordHash() {
  enterprise_password_hash_data_list_.reset();
}

void PasswordReuseDetector::ClearAllNonGmailPasswordHash() {
  if (!gaia_password_hash_data_list_)
    return;

  base::EraseIf(
      *gaia_password_hash_data_list_, [](const PasswordHashData& data) {
        std::string email =
            CanonicalizeUsername(data.username, data.is_gaia_password);
        return email.find("@gmail.com") == std::string::npos;
      });
}

void PasswordReuseDetector::AddPassword(const PasswordForm& form) {
  if (form.password_value.size() < kMinPasswordLengthToCheck)
    return;

  const auto result =
      passwords_with_matching_reused_credentials_[form.password_value].insert(
          {form.signon_realm, form.username_value, form.in_store});
  if (result.second) {
    saved_passwords_++;
  }
}

PasswordReuseDetector::passwords_iterator
PasswordReuseDetector::FindFirstSavedPassword(const base::string16& input) {
  // Keys in |passwords_with_matching_reused_credentials_| are ordered by
  // lexicographical order of reversed strings. In order to check a password
  // reuse a key of |passwords_with_matching_reused_credentials_| that is a
  // suffix of |input| should be found. The longest such key should be the
  // largest key in the |passwords_with_matching_reused_credentials_| keys order
  // that is equal or smaller to |input|. There may be more, shorter, matches as
  // well -- call FindNextSavedPassword(it) to find the next one.
  if (passwords_with_matching_reused_credentials_.empty())
    return passwords_with_matching_reused_credentials_.end();

  // lower_bound returns the first key that is bigger or equal to input.
  passwords_iterator it =
      passwords_with_matching_reused_credentials_.lower_bound(input);
  if (it != passwords_with_matching_reused_credentials_.end() &&
      it->first == input) {
    // If the key is equal then a saved password is found.
    return it;
  }
  // Otherwise the previous key is a candidate for password reuse.
  return FindNextSavedPassword(input, it);
}

PasswordReuseDetector::passwords_iterator
PasswordReuseDetector::FindNextSavedPassword(
    const base::string16& input,
    PasswordReuseDetector::passwords_iterator it) {
  if (it == passwords_with_matching_reused_credentials_.begin())
    return passwords_with_matching_reused_credentials_.end();
  --it;
  return IsSuffix(input, it->first)
             ? it
             : passwords_with_matching_reused_credentials_.end();
}

}  // namespace password_manager
