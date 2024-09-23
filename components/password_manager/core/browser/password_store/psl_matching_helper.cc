// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/psl_matching_helper.h"

#include <memory>
#include <ostream>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/password_manager/core/browser/password_form.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"
#include "url/url_constants.h"

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

bool IsFederatedRealm(const std::string& form_signon_realm, const GURL& url) {
  // The format should be "federation://origin.host/federation.host;
  std::string federated_realm = "federation://" + url.host() + "/";
  return form_signon_realm.size() > federated_realm.size() &&
         base::StartsWith(form_signon_realm, federated_realm,
                          base::CompareCase::INSENSITIVE_ASCII);
}

bool IsFederatedPSLMatch(const std::string& form_signon_realm,
                         const GURL& form_url,
                         const GURL& url) {
  if (!IsPublicSuffixDomainMatch(form_url.spec(), url.spec())) {
    return false;
  }

  return IsFederatedRealm(form_signon_realm, form_url);
}

MatchResult GetMatchResult(const PasswordForm& form,
                           const PasswordFormDigest& form_digest) {
  if (form.signon_realm == form_digest.signon_realm) {
    return MatchResult::EXACT_MATCH;
  }

  // PSL and federated matches only apply to HTML forms.
  if (form_digest.scheme != PasswordForm::Scheme::kHtml ||
      form.scheme != PasswordForm::Scheme::kHtml) {
    return MatchResult::NO_MATCH;
  }

  if (IsPublicSuffixDomainMatch(form.signon_realm, form_digest.signon_realm)) {
    return MatchResult::PSL_MATCH;
  }

  const bool allow_federated_match = form.federation_origin.IsValid();
  if (allow_federated_match &&
      IsFederatedRealm(form.signon_realm, form_digest.url) &&
      form.url.DeprecatedGetOriginAsURL() ==
          form_digest.url.DeprecatedGetOriginAsURL()) {
    return MatchResult::FEDERATED_MATCH;
  }

  if (allow_federated_match &&
      IsFederatedPSLMatch(form.signon_realm, form.url, form_digest.url)) {
    return MatchResult::FEDERATED_PSL_MATCH;
  }

  return MatchResult::NO_MATCH;
}

bool IsPublicSuffixDomainMatch(const std::string& url1,
                               const std::string& url2) {
  GURL gurl1(url1);
  GURL gurl2(url2);

  if (!gurl1.is_valid() || !gurl2.is_valid()) {
    return false;
  }

  if (gurl1 == gurl2) {
    return true;
  }

  if (gurl1.DomainIs("google.com") && gurl2.DomainIs("google.com")) {
    return gurl1.scheme() == gurl2.scheme() && gurl1.port() == gurl2.port() &&
           IsAllowedForPSLMatchedGoogleDomain(gurl1) &&
           IsAllowedForPSLMatchedGoogleDomain(gurl2);
  }

  std::string domain1(GetRegistryControlledDomain(gurl1));
  std::string domain2(GetRegistryControlledDomain(gurl2));

  if (domain1.empty() || domain2.empty()) {
    return false;
  }

  return gurl1.scheme() == gurl2.scheme() && domain1 == domain2 &&
         gurl1.port() == gurl2.port();
}

std::string GetRegistryControlledDomain(const GURL& signon_realm) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      signon_realm,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

std::string GetRegexForPSLMatching(const std::string& signon_realm) {
  const GURL signon_realm_url(signon_realm);
  std::string registered_domain = GetRegistryControlledDomain(signon_realm_url);
  DCHECK(!registered_domain.empty());
  // We are extending the original SQL query with one that includes more
  // possible matches based on public suffix domain matching. Using a regexp
  // here is just an optimization to not have to parse all the stored entries
  // in the |logins| table. The result (scheme, domain and port) is verified
  // further down using GURL. See the functions SchemeMatches,
  // RegistryControlledDomainMatches and PortMatches.
  // We need to escape . in the domain. Since the domain has already been
  // sanitized using GURL, we do not need to escape any other characters.
  base::ReplaceChars(registered_domain, ".", "\\.", &registered_domain);
  std::string scheme = signon_realm_url.scheme();
  // We need to escape . in the scheme. Since the scheme has already been
  // sanitized using GURL, we do not need to escape any other characters.
  // The scheme soap.beep is an example with '.'.
  base::ReplaceChars(scheme, ".", "\\.", &scheme);
  const std::string port = signon_realm_url.port();
  // For a signon realm such as http://foo.bar/, this regexp will match
  // domains on the form http://foo.bar/, http://www.foo.bar/,
  // http://www.mobile.foo.bar/. It will not match http://notfoo.bar/.
  // The scheme and port has to be the same as the observed form.
  return "^(" + scheme + ":\\/\\/)([\\w-]+\\.)*" + registered_domain +
         "(:" + port + ")?\\/$";
}

std::string GetRegexForPSLFederatedMatching(const std::string& signon_realm) {
  return "^federation://([\\w-]+\\.)*" +
         GetRegistryControlledDomain(GURL(signon_realm)) + "/.+$";
}

std::string GetExpressionForFederatedMatching(const GURL& url) {
  return base::StringPrintf("federation://%s/", url.host().c_str());
}

}  // namespace password_manager
