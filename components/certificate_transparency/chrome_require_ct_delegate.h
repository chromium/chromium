// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CERTIFICATE_TRANSPARENCY_CHROME_REQUIRE_CT_DELEGATE_H_
#define COMPONENTS_CERTIFICATE_TRANSPARENCY_CHROME_REQUIRE_CT_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "components/url_matcher/url_matcher.h"
#include "net/base/hash_value.h"
#include "net/http/transport_security_state.h"

namespace net {
class X509Certificate;
}  // namespace net

namespace certificate_transparency {

// ChromeRequireCTDelegate implements the policies used by Chrome to determine
// when to require Certificate Transparency for a host or certificate. Combined
// with ChromeCTPolicyEnforcer, these two classes implement the
// "Certificate Transparency in Chrome" policy from
// https://goo.gl/chrome/ct-policy - PolicyEnforcer imposing the policies on
// the SCTs to determine whether or not a certificate complies, and
// RequireCTDelegate to determine whether or not compliance is required for the
// connection to succeed.
//
// To support Enterprise configuration, additional requirements or exceptions
// can be provided via |UpdateCTPolicies()|, which uses the configuration
// syntax documented in pref_names.h for each of the options.
class COMPONENT_EXPORT(CERTIFICATE_TRANSPARENCY) ChromeRequireCTDelegate
    : public net::TransportSecurityState::RequireCTDelegate {
 public:
  explicit ChromeRequireCTDelegate();

  ChromeRequireCTDelegate(const ChromeRequireCTDelegate&) = delete;
  ChromeRequireCTDelegate& operator=(const ChromeRequireCTDelegate&) = delete;

  ~ChromeRequireCTDelegate() override;

  // RequireCTDelegate implementation
  CTRequirementLevel IsCTRequiredForHost(
      const std::string& hostname,
      const net::X509Certificate* chain,
      const net::HashValueVector& spki_hashes) override;

  // Updates the CTDelegate to require CT for |required_hosts|, and exclude
  // |excluded_hosts| from CT policies.  In addtion, this method updates
  // |excluded_spkis| and |excluded_legacy_spkis| intended for use within an
  // Enterprise (see https://crbug.com/824184).
  void UpdateCTPolicies(const std::vector<std::string>& required_hosts,
                        const std::vector<std::string>& excluded_hosts,
                        const std::vector<std::string>& excluded_spkis,
                        const std::vector<std::string>& excluded_legacy_spkis);

 private:
  struct Filter {
    bool ct_required = false;
    bool match_subdomains = false;
    size_t host_length = 0;
  };

  // Returns true if a policy for |hostname| is found, setting
  // |*ct_required| to indicate whether or not Certificate Transparency is
  // required for the host.
  bool MatchHostname(const std::string& hostname, bool* ct_required) const;

  // Returns true if a policy for |chain|, which contains the SPKI hashes
  // |hashes|, is found, setting |*ct_required| to indicate whether or not
  // Certificate Transparency is required for the certificate.
  bool MatchSPKI(const net::X509Certificate* chain,
                 const net::HashValueVector& hashes,
                 bool* ct_required) const;

  // Parses the filters from |host_patterns|, adding them as filters to
  // |filters_| (with |ct_required| indicating whether or not CT is required
  // for that host), and updating |*conditions| with the corresponding
  // URLMatcher::Conditions to match the host.
  void AddFilters(bool ct_required,
                  const std::vector<std::string>& host_patterns,
                  url_matcher::URLMatcherConditionSet::Vector* conditions);

  // Parses the SPKIs from |spki_list|, setting |*hashes| to the sorted set of
  // all valid SPKIs.
  void ParseSpkiHashes(const std::vector<std::string> spki_list,
                       net::HashValueVector* hashes) const;

  // Returns true if |lhs| has greater precedence than |rhs|. Filters of
  // higher precedence are consulted first when determining if a given filter
  // matches.
  bool FilterTakesPrecedence(const Filter& lhs, const Filter& rhs) const;

  std::unique_ptr<url_matcher::URLMatcher> url_matcher_;
  base::MatcherStringPattern::ID next_id_;
  std::map<base::MatcherStringPattern::ID, Filter> filters_;

  // Both SPKI lists are sorted.
  net::HashValueVector spkis_;
  net::HashValueVector legacy_spkis_;
};

}  // namespace certificate_transparency

#endif  // COMPONENTS_CERTIFICATE_TRANSPARENCY_CHROME_REQUIRE_CT_DELEGATE_H_
