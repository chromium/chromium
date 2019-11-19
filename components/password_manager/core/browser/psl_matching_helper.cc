// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/psl_matching_helper.h"

#include <memory>
#include <ostream>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/common/password_form.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using autofill::PasswordForm;

namespace password_manager {

namespace {
bool IsAllowedForPSLMatchedGoogleDomain(const GURL& url) {
  return url.DomainIs("myaccount.google.com") ||
         url.DomainIs("accounts.google.com");
}

}  // namespace

std::ostream& operator<<(std::ostream& out, MatchResult result) {
  switch (result) {
    case MatchResult::NO_MATCH:
      return out << "No Match";
    case MatchResult::EXACT_MATCH:
      return out << "Exact Match";
    case MatchResult::PSL_MATCH:
      return out << "PSL Match";
    case MatchResult::FEDERATED_MATCH:
      return out << "Federated Match";
    case MatchResult::FEDERATED_PSL_MATCH:
      return out << "Federated PSL Match";
  }
  // This should never be reached, it is simply here to suppress compiler
  // warnings.
  return out;
}

bool IsFederatedRealm(const std::string& form_signon_realm,
                      const GURL& origin) {
  // The format should be "federation://origin.host/federation.host;
  std::string federated_realm = "federation://" + origin.host() + "/";
  return form_signon_realm.size() > federated_realm.size() &&
         base::StartsWith(form_signon_realm, federated_realm,
                          base::CompareCase::INSENSITIVE_ASCII);
}

bool IsFederatedPSLMatch(const std::string& form_signon_realm,
                         const GURL& form_origin,
                         const GURL& origin) {
  if (!IsPublicSuffixDomainMatch(form_origin.spec(), origin.spec()))
    return false;

  return IsFederatedRealm(form_signon_realm, form_origin);
}

MatchResult GetMatchResult(const PasswordForm& form,
                           const PasswordStore::FormDigest& form_digest) {
  if (form.signon_realm == form_digest.signon_realm)
    return MatchResult::EXACT_MATCH;

  // PSL and federated matches only apply to HTML forms.
  if (form_digest.scheme != PasswordForm::Scheme::kHtml ||
      form.scheme != PasswordForm::Scheme::kHtml) {
    return MatchResult::NO_MATCH;
  }

  if (IsPublicSuffixDomainMatch(form.signon_realm, form_digest.signon_realm))
    return MatchResult::PSL_MATCH;

  const bool allow_federated_match = !form.federation_origin.opaque();
  if (allow_federated_match &&
      IsFederatedRealm(form.signon_realm, form_digest.origin) &&
      form.origin.GetOrigin() == form_digest.origin.GetOrigin()) {
    return MatchResult::FEDERATED_MATCH;
  }

  if (allow_federated_match &&
      IsFederatedPSLMatch(form.signon_realm, form.origin, form_digest.origin)) {
    return MatchResult::FEDERATED_PSL_MATCH;
  }

  return MatchResult::NO_MATCH;
}

bool IsPublicSuffixDomainMatch(const std::string& url1,
                               const std::string& url2) {
  GURL gurl1(url1);
  GURL gurl2(url2);

  if (!gurl1.is_valid() || !gurl2.is_valid())
    return false;

  if (gurl1 == gurl2)
    return true;

  if (gurl1.DomainIs("google.com") && gurl2.DomainIs("google.com")) {
    return gurl1.scheme() == gurl2.scheme() && gurl1.port() == gurl2.port() &&
           IsAllowedForPSLMatchedGoogleDomain(gurl1) &&
           IsAllowedForPSLMatchedGoogleDomain(gurl2);
  }

  std::string domain1(GetRegistryControlledDomain(gurl1));
  std::string domain2(GetRegistryControlledDomain(gurl2));

  if (domain1.empty() || domain2.empty())
    return false;

  return gurl1.scheme() == gurl2.scheme() && domain1 == domain2 &&
         gurl1.port() == gurl2.port();
}

std::string GetRegistryControlledDomain(const GURL& signon_realm) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      signon_realm,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

}  // namespace password_manager
