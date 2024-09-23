// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_MASKED_DOMAIN_LIST_MANAGER_H_
#define COMPONENTS_IP_PROTECTION_COMMON_MASKED_DOMAIN_LIST_MANAGER_H_

#include <map>
#include <optional>
#include <set>
#include <string>

#include "base/time/time.h"
#include "components/ip_protection/common/url_matcher_with_bypass.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/scheme_host_port_matcher.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom-forward.h"

namespace ip_protection {

// Class MaskedDomainListManager is a pseudo-singleton owned by the
// NetworkService. It uses the MaskedDomainList to generate the
// CustomProxyConfigPtr needed for NetworkContexts that are using the Privacy
// Proxy and determines if pairs of request and top_frame URLs are eligible.
class MaskedDomainListManager {
 public:
  explicit MaskedDomainListManager(
      network::mojom::IpProtectionProxyBypassPolicy);
  ~MaskedDomainListManager();
  MaskedDomainListManager(const MaskedDomainListManager&);

  static MaskedDomainListManager CreateForTesting(
      const std::map<std::string, std::set<std::string>>& first_party_map);

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

  // Returns true if the allow list is eligible to be used but does not indicate
  // that the allow list is currently populated.
  bool IsEnabled() const;

  // Returns true if there are entries in the allow list and it is possible to
  // match on them. If false, `Matches` will always return false.
  bool IsPopulated() const;

  // Determines if the request is eligible for the proxy by determining if the
  // request_url is for an eligible domain and if the NAK supports eligibility.
  // If the top_frame_origin of the NAK does not have the same owner as the
  // request_url and the request_url is in the allow list, the request is
  // eligible for the proxy.
  // TODO(crbug.com/354649091): Public Suffix List domains and subdomains
  // proxy 1st party requests because no same-origin check is performed.
  bool Matches(
      const GURL& request_url,
      const net::NetworkAnonymizationKey& network_anonymization_key) const;

  // Use the Masked Domain List and exclusion list to generate the allow list
  // and the 1P bypass rules.
  void UpdateMaskedDomainList(const masked_domain_list::MaskedDomainList& mdl,
                              const std::vector<std::string>& exclusion_list);

 private:
  FRIEND_TEST_ALL_PREFIXES(MaskedDomainListManagerBaseTest,
                           ExclusionSetDomainsRemovedFromMDL);

  // Removes domains from the MDL which are either part of the exclusion list
  // themselves or are subdomains of an entry in the exclusion list.
  // Returns MDL after removing such domains.
  std::set<std::string> ExcludeDomainsFromMDL(
      const std::set<std::string>& mdl_domains,
      const std::set<std::string>& excluded_domains);

  // Determines whether or not the resource URL matches any URL listed in the
  // public suffix list.
  bool MatchesPublicSuffixList(const GURL& resource_url) const;

  // Add domains to the `public_suffix_list_matcher_`.
  void AddPublicSuffixListRules(const std::set<std::string>& domains);

  // Policy that determines which domains are bypassed from IP Protection.
  network::mojom::IpProtectionProxyBypassPolicy proxy_bypass_policy_;

  // Contains match rules from the Masked Domain List.
  UrlMatcherWithBypass url_matcher_with_bypass_;

  // Matcher which matches against the public suffix list domains.
  net::SchemeHostPortMatcher public_suffix_list_matcher_;

  // If UpdateMaskedDomainList has not yet been called, stores the time at which
  // the manager was created. The first call to UpdateMaskedDomainList clears
  // this to nullopt on entry.
  std::optional<base::TimeTicks> creation_time_for_mdl_update_metric_;
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_MASKED_DOMAIN_LIST_MANAGER_H_
