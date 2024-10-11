// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_reuse_detector_impl.h"

#include <algorithm>
#include <optional>
#include <set>
#include <utility>

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_hash_data.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/password_manager/core/browser/password_reuse_detector_consumer.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/psl_matching_helper.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "url/gurl.h"

namespace password_manager {

namespace {
// Returns true iff |suffix_candidate| is a suffix of |str|.
bool IsSuffix(const std::u16string& str,
              const std::u16string& suffix_candidate) {
  if (str.size() < suffix_candidate.size()) {
    return false;
  }
  return std::equal(suffix_candidate.rbegin(), suffix_candidate.rend(),
                    str.rbegin());
}

// Helper function to returns matching PasswordHashData from a list that has
// the longest password length.
std::optional<PasswordHashData> FindPasswordReuse(
    const std::u16string& input,
    const std::vector<PasswordHashData>& password_hash_list) {
  std::optional<PasswordHashData> longest_match = std::nullopt;
  size_t longest_match_size = 0;
  for (const PasswordHashData& hash_data : password_hash_list) {
    if (input.size() < hash_data.length) {
      continue;
    }
    size_t offset = input.size() - hash_data.length;
    std::u16string reuse_candidate = input.substr(offset);
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

bool ReverseStringLess::operator()(const std::u16string& lhs,
                                   const std::u16string& rhs) const {
  return std::lexicographical_compare(lhs.rbegin(), lhs.rend(), rhs.rbegin(),
                                      rhs.rend());
}

PasswordReuseDetectorImpl::PasswordLengthAndMatchingCredentials::
    PasswordLengthAndMatchingCredentials() = default;
PasswordReuseDetectorImpl::PasswordLengthAndMatchingCredentials::
    ~PasswordLengthAndMatchingCredentials() = default;

PasswordReuseDetectorImpl::PasswordReuseDetectorImpl()
    : salt_(CreateRandomSalt()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

PasswordReuseDetectorImpl::~PasswordReuseDetectorImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PasswordReuseDetectorImpl::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& form : results) {
    AddPassword(*form);
  }
}

void PasswordReuseDetectorImpl::OnLoginsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& change : changes) {
    if (change.type() == PasswordStoreChange::ADD ||
        change.type() == PasswordStoreChange::UPDATE) {
      AddPassword(change.form());
    }
    if (change.type() == PasswordStoreChange::REMOVE) {
      RemovePassword(change.form());
    }
  }
}

void PasswordReuseDetectorImpl::OnLoginsRetained(
    PasswordForm::Store password_store_type,
    const std::vector<PasswordForm>& retained_passwords) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveAllLoginsByStoreType(password_store_type);

  // |retained_passwords| contains also blacklisted entities, but since they
  // don't have password value they will be skipped inside AddPassword().
  for (const auto& form : retained_passwords) {
    AddPassword(form);
  }
}

void PasswordReuseDetectorImpl::ClearCachedAccountStorePasswords() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveAllLoginsByStoreType(PasswordForm::Store::kAccountStore);
}

void PasswordReuseDetectorImpl::CheckReuse(
    const std::u16string& input,
    const std::string& domain,
    PasswordReuseDetectorConsumer* consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(consumer);
  if (input.size() < kMinPasswordLengthToCheck) {
    consumer->OnReuseCheckDone(false, 0, std::nullopt, {},
                               SavedPasswordsCount(), std::string(), 0);
    return;
  }

  std::optional<PasswordHashData> reused_gaia_password_hash =
      CheckGaiaPasswordReuse(input, domain);
  size_t gaia_reused_password_length = reused_gaia_password_hash.has_value()
                                           ? reused_gaia_password_hash->length
                                           : 0;

  std::optional<PasswordHashData> reused_enterprise_password_hash =
      CheckNonGaiaEnterprisePasswordReuse(input, domain);
  size_t enterprise_reused_password_length =
      reused_enterprise_password_hash.has_value()
          ? reused_enterprise_password_hash->length
          : 0;

  std::vector<MatchingReusedCredential> matching_reused_credentials;

  std::pair<uint64_t, size_t> hash_and_pwd_len =
      base::FeatureList::IsEnabled(
          features::kReuseDetectionBasedOnPasswordHashes)
          ? CheckSavedPasswordReuseBasedOnHash(input, domain,
                                               &matching_reused_credentials)
          : CheckSavedPasswordReuse(input, domain,
                                    &matching_reused_credentials);

  size_t max_reused_password_length =
      std::max({hash_and_pwd_len.second, gaia_reused_password_length,
                enterprise_reused_password_length});

  if (max_reused_password_length == 0) {
    consumer->OnReuseCheckDone(false, 0, std::nullopt, {},
                               SavedPasswordsCount(), std::string(), 0);
    return;
  }

  uint64_t reused_password_hash = hash_and_pwd_len.first;

  std::optional<PasswordHashData> reused_protected_password_hash = std::nullopt;
  if (gaia_reused_password_length > enterprise_reused_password_length) {
    reused_password_hash = reused_gaia_password_hash->hash;
    reused_protected_password_hash = std::move(reused_gaia_password_hash);
  } else if (enterprise_reused_password_length != 0) {
    reused_password_hash = reused_enterprise_password_hash->hash;
    reused_protected_password_hash = std::move(reused_enterprise_password_hash);
  }

  consumer->OnReuseCheckDone(true, max_reused_password_length,
                             reused_protected_password_hash,
                             matching_reused_credentials, SavedPasswordsCount(),
                             domain, reused_password_hash);
}

std::optional<PasswordHashData>
PasswordReuseDetectorImpl::CheckGaiaPasswordReuse(const std::u16string& input,
                                                  const std::string& domain) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!gaia_password_hash_data_list_.has_value() ||
      gaia_password_hash_data_list_->empty()) {
    return std::nullopt;
  }

  // Skips password reuse check if |domain| matches Gaia origin.
  if (gaia::HasGaiaSchemeHostPort(GURL(domain))) {
    return std::nullopt;
  }

  return FindPasswordReuse(input, gaia_password_hash_data_list_.value());
}

std::optional<PasswordHashData>
PasswordReuseDetectorImpl::CheckNonGaiaEnterprisePasswordReuse(
    const std::u16string& input,
    const std::string& domain) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!enterprise_password_hash_data_list_.has_value() ||
      enterprise_password_hash_data_list_->empty()) {
    return std::nullopt;
  }

  // Skips password reuse check if |domain| matches enterprise login URL or
  // enterprise change password URL.
  GURL page_url(domain);
  if (enterprise_password_urls_.has_value() &&
      safe_browsing::MatchesURLList(page_url,
                                    enterprise_password_urls_.value())) {
    return std::nullopt;
  }

  return FindPasswordReuse(input, enterprise_password_hash_data_list_.value());
}

std::pair<uint64_t, size_t>
PasswordReuseDetectorImpl::CheckSavedPasswordReuseBasedOnHash(
    const std::u16string& input,
    const std::string& domain,
    std::vector<MatchingReusedCredential>* matching_reused_credentials_out) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The longest password match is kept for metrics.
  size_t longest_match_len = 0;

  // Hash of the longest reused password.
  uint64_t reused_saved_password_hash;

  // Collection of all credentials for which password reuse is detected.
  std::set<MatchingReusedCredential> matching_reused_credentials_set;

  // All possible password lengths.
  std::set<size_t> pwd_lengths;

  for (const auto& it : password_hashes_with_matching_reused_credentials_) {
    pwd_lengths.insert(it.second.password_length);
  }

  const std::string registry_controlled_domain =
      GetRegistryControlledDomain(GURL(domain));

  // Goes over all possible password lengths and checks input suffix of that
  // length against known password hashes.
  for (auto len : pwd_lengths) {
    if (input.size() < len) {
      continue;
    }
    std::u16string_view reuse_candidate =
        std::u16string_view(input).substr(input.size() - len);
    uint64_t input_hash = CalculatePasswordHash(reuse_candidate, salt_);

    auto passwords_iterator =
        password_hashes_with_matching_reused_credentials_.find(input_hash);
    if (passwords_iterator ==
        password_hashes_with_matching_reused_credentials_.end()) {
      continue;
    }

    const std::set<MatchingReusedCredential>& credentials =
        passwords_iterator->second.matching_credentials;
    CHECK(!credentials.empty());

    std::set<std::string> domains;
    for (const auto& credential : credentials) {
      domains.insert(GetRegistryControlledDomain(credential.url));
    }
    // If the page's URL matches a saved domain for this password,
    // this isn't password-reuse.
    if (base::Contains(domains, registry_controlled_domain)) {
      continue;
    }

    matching_reused_credentials_set.insert(credentials.begin(),
                                           credentials.end());
    if (len > longest_match_len) {
      longest_match_len = len;
      reused_saved_password_hash = input_hash;
    }
  }

  matching_reused_credentials_out->reserve(
      matching_reused_credentials_set.size());
  for (auto it = matching_reused_credentials_set.begin();
       it != matching_reused_credentials_set.end();) {
    matching_reused_credentials_out->push_back(
        std::move(matching_reused_credentials_set.extract(it++).value()));
  }

  return {reused_saved_password_hash, longest_match_len};
}

std::pair<uint64_t, size_t> PasswordReuseDetectorImpl::CheckSavedPasswordReuse(
    const std::u16string& input,
    const std::string& domain,
    std::vector<MatchingReusedCredential>* matching_reused_credentials_out) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::string registry_controlled_domain =
      GetRegistryControlledDomain(GURL(domain));

  // More than one password call match |input| if they share a common suffix
  // with |input|.  Collect the set of MatchingReusedCredential for all matches.
  std::set<MatchingReusedCredential> matching_reused_credentials_set;

  // The longest password match is kept for metrics.
  size_t longest_match_len = 0;

  // The reused saved password.
  std::u16string reused_saved_password;

  for (auto passwords_iterator = FindFirstSavedPassword(input);
       passwords_iterator != passwords_with_matching_reused_credentials_.end();
       passwords_iterator = FindNextSavedPassword(input, passwords_iterator)) {
    const std::set<MatchingReusedCredential>& credentials =
        passwords_iterator->second;
    DCHECK(!credentials.empty());

    std::set<std::string> domains;
    for (const auto& credential : credentials) {
      domains.insert(GetRegistryControlledDomain(credential.url));
    }
    // If the page's URL matches a saved domain for this password,
    // this isn't password-reuse.
    if (base::Contains(domains, registry_controlled_domain)) {
      continue;
    }

    matching_reused_credentials_set.insert(credentials.begin(),
                                           credentials.end());
    DCHECK(!passwords_iterator->first.empty());
    if (passwords_iterator->first.size() > longest_match_len) {
      longest_match_len = passwords_iterator->first.size();
      reused_saved_password = passwords_iterator->first;
    }
  }

  matching_reused_credentials_out->reserve(
      matching_reused_credentials_set.size());
  for (auto it = matching_reused_credentials_set.begin();
       it != matching_reused_credentials_set.end();) {
    matching_reused_credentials_out->push_back(
        std::move(matching_reused_credentials_set.extract(it++).value()));
  }

  if (longest_match_len != 0) {
    return {CalculatePasswordHash(reused_saved_password, std::string()),
            longest_match_len};
  }

  return {0, 0};
}

void PasswordReuseDetectorImpl::UseGaiaPasswordHash(
    std::optional<std::vector<PasswordHashData>> password_hash_data_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  gaia_password_hash_data_list_ = std::move(password_hash_data_list);
}

void PasswordReuseDetectorImpl::UseNonGaiaEnterprisePasswordHash(
    std::optional<std::vector<PasswordHashData>> password_hash_data_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  enterprise_password_hash_data_list_ = std::move(password_hash_data_list);
}

void PasswordReuseDetectorImpl::UseEnterprisePasswordURLs(
    std::optional<std::vector<GURL>> enterprise_login_urls,
    std::optional<GURL> enterprise_change_password_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  enterprise_password_urls_ = std::move(enterprise_login_urls);
  if (!enterprise_change_password_url.has_value() ||
      !enterprise_change_password_url->is_valid()) {
    return;
  }

  if (!enterprise_password_urls_) {
    enterprise_password_urls_ = std::make_optional<std::vector<GURL>>();
  }
  enterprise_password_urls_->push_back(enterprise_change_password_url.value());
}

void PasswordReuseDetectorImpl::ClearGaiaPasswordHash(
    const std::string& username) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!gaia_password_hash_data_list_) {
    return;
  }

  std::erase_if(*gaia_password_hash_data_list_,
                [&username](const PasswordHashData& data) {
                  return AreUsernamesSame(username, true, data.username,
                                          data.is_gaia_password);
                });
}

void PasswordReuseDetectorImpl::ClearAllGaiaPasswordHash() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  gaia_password_hash_data_list_.reset();
}

void PasswordReuseDetectorImpl::ClearAllEnterprisePasswordHash() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  enterprise_password_hash_data_list_.reset();
}

void PasswordReuseDetectorImpl::ClearAllNonGmailPasswordHash() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!gaia_password_hash_data_list_) {
    return;
  }

  std::erase_if(
      *gaia_password_hash_data_list_, [](const PasswordHashData& data) {
        std::string email =
            CanonicalizeUsername(data.username, data.is_gaia_password);
        return email.find("@gmail.com") == std::string::npos;
      });
}

void PasswordReuseDetectorImpl::AddPassword(const PasswordForm& form) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (form.password_value.size() < kMinPasswordLengthToCheck) {
    return;
  }

  if (base::FeatureList::IsEnabled(
          features::kReuseDetectionBasedOnPasswordHashes)) {
    uint64_t password_hash = CalculatePasswordHash(form.password_value, salt_);
    password_hashes_with_matching_reused_credentials_[password_hash]
        .matching_credentials.insert(MatchingReusedCredential(form));
    password_hashes_with_matching_reused_credentials_[password_hash]
        .password_length = form.password_value.size();
  } else {
    passwords_with_matching_reused_credentials_[form.password_value].insert(
        MatchingReusedCredential(form));
  }
}

void PasswordReuseDetectorImpl::RemovePassword(const PasswordForm& form) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (form.password_value.size() < kMinPasswordLengthToCheck) {
    return;
  }

  MatchingReusedCredential credential_criteria(form);
  auto password_value_iter =
      passwords_with_matching_reused_credentials_.begin();
  while (password_value_iter !=
         passwords_with_matching_reused_credentials_.end()) {
    std::set<MatchingReusedCredential>& stored_credentials_for_password_value =
        password_value_iter->second;
    // Remove only the password for the specific domain and username.
    // Don't remove all passwords from
    // |passwords_with_matching_reused_credentials_| with a given
    // |form.password_value|.
    const auto credential_to_remove =
        stored_credentials_for_password_value.find(credential_criteria);
    if (credential_to_remove != stored_credentials_for_password_value.end()) {
      stored_credentials_for_password_value.erase(credential_to_remove);

      // If all credential values of the password key are deleted, remove the
      // password key from the map.
      if (stored_credentials_for_password_value.empty()) {
        password_value_iter = passwords_with_matching_reused_credentials_.erase(
            password_value_iter);
      }
    } else {
      password_value_iter++;
    }
  }

  if (!base::FeatureList::IsEnabled(
          features::kReuseDetectionBasedOnPasswordHashes)) {
    return;
  }
  uint64_t password_hash = CalculatePasswordHash(form.password_value, salt_);
  auto password_hash_iterator =
      password_hashes_with_matching_reused_credentials_.find(password_hash);
  if (password_hash_iterator ==
      password_hashes_with_matching_reused_credentials_.end()) {
    return;
  }

  std::set<MatchingReusedCredential>& stored_credentials_for_password_value =
      password_hash_iterator->second.matching_credentials;
  const auto credential_to_remove =
      stored_credentials_for_password_value.find(credential_criteria);
  if (credential_to_remove != stored_credentials_for_password_value.end()) {
    stored_credentials_for_password_value.erase(credential_to_remove);

    // If all credential values of the password key are deleted, remove the
    // password key from the map.
    if (stored_credentials_for_password_value.empty()) {
      password_hashes_with_matching_reused_credentials_.erase(
          password_hash_iterator);
    }
  }
}

void PasswordReuseDetectorImpl::RemoveAllLoginsByStoreType(
    PasswordForm::Store store_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto i = passwords_with_matching_reused_credentials_.begin();
       i != passwords_with_matching_reused_credentials_.end();) {
    // Remove all the matching credentials from the corresponding store.
    std::erase_if(
        i->second,
        [store_type](const MatchingReusedCredential& matched_credential) {
          return matched_credential.in_store == store_type;
        });
    // Remove the map entry if there are no matching credentials left for this
    // password.
    if (i->second.empty()) {
      passwords_with_matching_reused_credentials_.erase(i++);
    } else {
      ++i;
    }
  }
  if (!base::FeatureList::IsEnabled(
          features::kReuseDetectionBasedOnPasswordHashes)) {
    return;
  }
  for (auto i = password_hashes_with_matching_reused_credentials_.begin();
       i != password_hashes_with_matching_reused_credentials_.end();) {
    // Remove all the matching credentials from the corresponding store.
    std::erase_if(
        i->second.matching_credentials,
        [store_type](const MatchingReusedCredential& matched_credential) {
          return matched_credential.in_store == store_type;
        });
    // Remove the map entry if there are no matching credentials left for this
    // password.
    if (i->second.matching_credentials.empty()) {
      password_hashes_with_matching_reused_credentials_.erase(i++);
    } else {
      ++i;
    }
  }
}

PasswordReuseDetectorImpl::passwords_iterator
PasswordReuseDetectorImpl::FindFirstSavedPassword(const std::u16string& input) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Keys in |passwords_with_matching_reused_credentials_| are ordered by
  // lexicographical order of reversed strings. In order to check a password
  // reuse a key of |passwords_with_matching_reused_credentials_| that is a
  // suffix of |input| should be found. The longest such key should be the
  // largest key in the |passwords_with_matching_reused_credentials_| keys order
  // that is equal or smaller to |input|. There may be more, shorter, matches as
  // well -- call FindNextSavedPassword(it) to find the next one.
  if (passwords_with_matching_reused_credentials_.empty()) {
    return passwords_with_matching_reused_credentials_.end();
  }

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

PasswordReuseDetectorImpl::passwords_iterator
PasswordReuseDetectorImpl::FindNextSavedPassword(
    const std::u16string& input,
    PasswordReuseDetectorImpl::passwords_iterator it) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (it == passwords_with_matching_reused_credentials_.begin()) {
    return passwords_with_matching_reused_credentials_.end();
  }
  --it;
  return IsSuffix(input, it->first)
             ? it
             : passwords_with_matching_reused_credentials_.end();
}

size_t PasswordReuseDetectorImpl::SavedPasswordsCount() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  size_t count = 0;
  if (base::FeatureList::IsEnabled(
          features::kReuseDetectionBasedOnPasswordHashes)) {
    for (const auto& pair : password_hashes_with_matching_reused_credentials_) {
      count += pair.second.matching_credentials.size();
    }
  } else {
    for (const auto& pair : passwords_with_matching_reused_credentials_) {
      count += pair.second.size();
    }
  }

  return count;
}

}  // namespace password_manager
