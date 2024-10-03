// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/masked_domain_list_manager.h"

#include <set>
#include <vector>

#include "base/containers/contains.h"
#include "base/time/time.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
#include "components/ip_protection/common/url_matcher_with_bypass.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/features.h"
#include "url/url_constants.h"

namespace ip_protection {
namespace {
using ::masked_domain_list::PublicSuffixListRule;
using ::masked_domain_list::Resource;
using ::masked_domain_list::ResourceOwner;
using ::network::mojom::IpProtectionProxyBypassPolicy;

// Returns a `ResourceOwner` identical to the input `resource_owner` but without
// any `owned_resource` exactly matching a private domain listed in the PSL.
ResourceOwner RemovePslPrivateDomainsFromOwnedResources(
    const std::set<std::string>& psl_private_domains,
    const ResourceOwner& resource_owner) {
  ResourceOwner res = resource_owner;
  res.clear_owned_resources();
  for (const Resource& owned_resource : resource_owner.owned_resources()) {
    if (psl_private_domains.contains(owned_resource.domain())) {
      continue;
    }
    *res.add_owned_resources() = owned_resource;
  }
  return res;
}

}  // namespace

MaskedDomainListManager::MaskedDomainListManager(
    IpProtectionProxyBypassPolicy policy)
    : proxy_bypass_policy_{policy},
      creation_time_for_mdl_update_metric_(base::TimeTicks::Now()) {}

MaskedDomainListManager::~MaskedDomainListManager() = default;

MaskedDomainListManager::MaskedDomainListManager(
    const MaskedDomainListManager&) {}

MaskedDomainListManager MaskedDomainListManager::CreateForTesting(
    const std::map<std::string, std::set<std::string>>& first_party_map) {
  auto allow_list = MaskedDomainListManager(
      IpProtectionProxyBypassPolicy::kFirstPartyToTopLevelFrame);

  auto mdl = masked_domain_list::MaskedDomainList();

  for (auto const& [domain, properties] : first_party_map) {
    ResourceOwner& resourceOwner = *mdl.add_resource_owners();
    for (auto property : properties) {
      resourceOwner.add_owned_properties(property);
    }
    Resource& resource = *resourceOwner.add_owned_resources();
    resource.set_domain(domain);
  }

  allow_list.UpdateMaskedDomainList(
      mdl,
      /*exclusion_list=*/std::vector<std::string>());
  return allow_list;
}

bool MaskedDomainListManager::IsEnabled() const {
  return base::FeatureList::IsEnabled(network::features::kMaskedDomainList);
}

bool MaskedDomainListManager::IsPopulated() const {
  return url_matcher_with_bypass_.IsPopulated() ||
         !public_suffix_list_matcher_.rules().empty();
}

size_t MaskedDomainListManager::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(url_matcher_with_bypass_);
}

bool MaskedDomainListManager::Matches(
    const GURL& request_url,
    const net::NetworkAnonymizationKey& network_anonymization_key) const {
  std::optional<net::SchemefulSite> top_frame_site =
      network_anonymization_key.GetTopFrameSite();
  UrlMatcherWithBypassResult match_result;
  switch (proxy_bypass_policy_) {
    case IpProtectionProxyBypassPolicy::kNone:
    case IpProtectionProxyBypassPolicy::kExclusionList:
      match_result =
          url_matcher_with_bypass_.Matches(request_url, top_frame_site,
                                           /*skip_bypass_check=*/true);
      break;
    case IpProtectionProxyBypassPolicy::kFirstPartyToTopLevelFrame:
      if (!top_frame_site.has_value()) {
        VLOG(3) << "MDLM::Matches(" << request_url
                << ", empty top_frame_site) - false";
        return false;
      }
      VLOG(3) << "MDLM::Matches(" << request_url << ", "
              << top_frame_site.value() << ")";

      // Bypass the proxy for same-site requests.
      net::SchemefulSite request_site(request_url);
      if (top_frame_site.has_value() && top_frame_site == request_site) {
        return false;
      }

      // Only proxy traffic where the top-level site is an HTTP/HTTPS page or
      // where the NAK corresponds to a fenced frame.
      if (net::features::kIpPrivacyRestrictTopLevelSiteSchemes.Get() &&
          !network_anonymization_key.GetNonce().has_value() &&
          !top_frame_site.value().GetURL().SchemeIsHTTPOrHTTPS()) {
        // Note: It's possible that the top-level site could be a file: URL in
        // the case where an HTML file was downloaded and then opened. We don't
        // proxy in this case in favor of better compatibility. It's also
        // possible that the top-level site could be a blob URL, data URL, or
        // filesystem URL (the latter two with restrictions on how they could
        // have been navigated to), but we'll assume these aren't used
        // pervasively as the top-level site for pages that make the types of
        // requests that IP Protection will apply to.
        return false;
      }

      // If the NAK is transient (has a nonce and/or top_frame_origin is
      // opaque), we should skip the first party check and match only on the
      // request_url.
      match_result = url_matcher_with_bypass_.Matches(
          request_url, top_frame_site, network_anonymization_key.IsTransient());
      break;
  }
  switch (match_result) {
    case UrlMatcherWithBypassResult::kMatchAndBypass:
      return false;
    case UrlMatcherWithBypassResult::kMatchAndNoBypass:
      return true;
    case UrlMatcherWithBypassResult::kNoMatch:
      // The resource could have been listed in the Public Suffix List and
      // been removed from the MDL's owned resources. Requests to it should be
      // proxied if it was present in the PSL (i.e. matched by the PSL matcher),
      // as those domains and their subdomains should always be considered
      // third-party.
      return MatchesPublicSuffixList(request_url);
  }
}

std::set<std::string> MaskedDomainListManager::ExcludeDomainsFromMDL(
    const std::set<std::string>& mdl_domains,
    const std::set<std::string>& excluded_domains) {
  if (excluded_domains.empty()) {
    return mdl_domains;
  }

  std::set<std::string> mdl_domains_after_exclusions;
  for (const auto& mdl_domain : mdl_domains) {
    std::string mdl_superdomain(mdl_domain);
    bool shouldInclude = true;

    // Check if any super domains exist in the exclusion set
    // For example, mdl_domain W.X.Y.Z should be excluded if exclusion set
    // contains super domains W.X.Y.Z or X.Y.Z or Y.Z or Z

    // TODO(crbug/326399905): Add logic for excluding a domain X if any other
    // domain owned by X's resource owner is on the exclusion list.

    while (!mdl_superdomain.empty()) {
      if (base::Contains(excluded_domains, mdl_superdomain)) {
        shouldInclude = false;
        break;
      }
      mdl_superdomain = net::GetSuperdomain(mdl_superdomain);
    }

    if (shouldInclude) {
      mdl_domains_after_exclusions.insert(mdl_domain);
    }
  }

  return mdl_domains_after_exclusions;
}

void MaskedDomainListManager::UpdateMaskedDomainList(
    const masked_domain_list::MaskedDomainList& mdl,
    const std::vector<std::string>& exclusion_list) {
  if (creation_time_for_mdl_update_metric_.has_value()) {
    Telemetry().MdlFirstUpdateTime(base::TimeTicks::Now() -
                                   *creation_time_for_mdl_update_metric_);
    creation_time_for_mdl_update_metric_.reset();
  }

  const int experiment_group_id =
      network::features::kMaskedDomainListExperimentGroup.Get();
  // Clients are in the default group if the experiment_group_id is the
  // default value of 0.
  const bool in_default_group = (experiment_group_id == 0);

  // All Resources are used by the default group unless they are explicitly
  // excluded. For a client in the experiment group to use a Resource, the
  // Resource must explicitly list the experiment group.
  auto is_eligible = [&](masked_domain_list::Resource resource) {
    if (in_default_group) {
      return !resource.exclude_default_group();
    }
    return base::Contains(resource.experiment_group_ids(), experiment_group_id);
  };

  const std::set<std::string> exclusion_set(exclusion_list.begin(),
                                            exclusion_list.end());

  // Collect the PSL private domains in a set for efficient querying.
  std::set<std::string> psl_private_domains;
  std::transform(
      mdl.public_suffix_list_rules().begin(),
      mdl.public_suffix_list_rules().end(),
      std::inserter(psl_private_domains, psl_private_domains.end()),
      [](const PublicSuffixListRule& pslr) { return pslr.private_domain(); });

  url_matcher_with_bypass_.Clear();
  public_suffix_list_matcher_.Clear();
  for (const ResourceOwner& owner : mdl.resource_owners()) {
    // Group domains by partition first so that only one set of the owner's
    // bypass rules are created per partition.
    std::set<std::string> eligible_domains;

    // Domains that are listed in the public suffix list should not be added to
    // url_matcher_with_bypass_, because they are not actually owned.
    // TODO(b/328788380): Client-side removal is only temporarily necessary,
    // until support for the new PSL field in the MDL is rolled out to chrome
    // stable and the adjustments to the MDL can be made at source.
    const ResourceOwner owner_without_psl_owned_properties =
        RemovePslPrivateDomainsFromOwnedResources(psl_private_domains, owner);

    for (const Resource& resource :
         owner_without_psl_owned_properties.owned_resources()) {
      if (is_eligible(resource)) {
        eligible_domains.insert(resource.domain());
      }
    }

    switch (proxy_bypass_policy_) {
      case IpProtectionProxyBypassPolicy::kNone: {
        url_matcher_with_bypass_.AddRulesWithoutBypass(eligible_domains);
        break;
      }
      case IpProtectionProxyBypassPolicy::kFirstPartyToTopLevelFrame: {
        url_matcher_with_bypass_.AddMaskedDomainListRules(
            eligible_domains, owner_without_psl_owned_properties);
        break;
      }
      case IpProtectionProxyBypassPolicy::kExclusionList: {
        url_matcher_with_bypass_.AddRulesWithoutBypass(
            ExcludeDomainsFromMDL(eligible_domains, exclusion_set));
        break;
      }
    }
  }
  AddPublicSuffixListRules(psl_private_domains);
  Telemetry().MdlEstimatedMemoryUsage(EstimateMemoryUsage());
}

bool MaskedDomainListManager::MatchesPublicSuffixList(
    const GURL& resource_url) const {
  return public_suffix_list_matcher_.Evaluate(resource_url) !=
         net::SchemeHostPortMatcherResult::kNoMatch;
}

void MaskedDomainListManager::AddPublicSuffixListRules(
    const std::set<std::string>& domains) {
  for (const std::string& domain : domains) {
    auto domain_rule =
        net::SchemeHostPortMatcherRule::FromUntrimmedRawString(domain);

    if (domain_rule) {
      public_suffix_list_matcher_.AddAsLastRule(std::move(domain_rule));
    } else {
      return;
    }

    const bool domain_covers_subdomains =
        domain.starts_with(".") || domain.starts_with("*");
    if (!domain_covers_subdomains) {
      // Domain doesn't cover its subdomains so add them manually.
      std::string subdomain = base::StrCat({".", domain});
      auto subdomain_rule =
          net::SchemeHostPortMatcherRule::FromUntrimmedRawString(subdomain);

      if (subdomain_rule) {
        public_suffix_list_matcher_.AddAsLastRule(std::move(subdomain_rule));
      } else {
        return;
      }
    }
  }
}

}  // namespace ip_protection
