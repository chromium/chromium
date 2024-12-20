// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/url_matcher_with_bypass.h"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "net/base/scheme_host_port_matcher.h"
#include "net/base/schemeful_site.h"
#include "net/base/url_util.h"
#include "url_matcher_with_bypass.h"

namespace ip_protection {

namespace {
using ::masked_domain_list::Resource;
using ::masked_domain_list::ResourceOwner;

bool HasSubdomainCoverage(std::string_view domain) {
  return domain.starts_with(".") || domain.starts_with("*");
}

void AddRulesToMatcher(std::string_view domain,
                       const bool include_subdomains,
                       net::SchemeHostPortMatcher& matcher) {
  auto domain_rule =
      net::SchemeHostPortMatcherRule::FromUntrimmedRawString(domain);

  if (domain_rule) {
    matcher.AddAsLastRule(std::move(domain_rule));
  } else {
    VLOG(3) << "UrlMatcherWithBypass::UpdateMatcher() - " << domain
            << " is not a valid rule";
    return;
  }

  if (include_subdomains) {
    std::string subdomain = base::StrCat({".", domain});
    auto subdomain_rule =
        net::SchemeHostPortMatcherRule::FromUntrimmedRawString(subdomain);

    if (subdomain_rule) {
      matcher.AddAsLastRule(std::move(subdomain_rule));
    } else {
      VLOG(3) << "UrlMatcherWithBypass::UpdateMatcher() - " << subdomain
              << " is not a valid rule";
      return;
    }
  }
}

std::map<std::string, std::set<std::string>> PartitionDomains(
    const std::set<std::string>& domains) {
  std::map<std::string, std::set<std::string>> domains_by_partition;

  for (auto domain : domains) {
    const std::string partition = UrlMatcherWithBypass::PartitionMapKey(domain);
    domains_by_partition[partition].insert(domain);
  }
  return domains_by_partition;
}

// TODO(crbug.com/326399905): Add logic for excluding a domain X if any other
// domain owned by X's resource owner is on the exclusion list.
std::set<std::string> ExcludeDomainsFromMDL(
    const std::set<std::string>& mdl_domains,
    const std::unordered_set<std::string>& excluded_domains) {
  if (excluded_domains.empty()) {
    return mdl_domains;
  }

  std::set<std::string> filtered_domains;
  for (const auto& mdl_domain : mdl_domains) {
    std::string mdl_superdomain(mdl_domain);

    bool shouldInclude = true;

    // Exclude mdl_domain if any of its superdomains are in excluded_domains.
    while (!mdl_superdomain.empty()) {
      if (excluded_domains.contains(mdl_superdomain)) {
        shouldInclude = false;
        break;
      }
      mdl_superdomain = net::GetSuperdomain(mdl_superdomain);
    }

    if (shouldInclude) {
      filtered_domains.insert(mdl_domain);
    }
  }

  return filtered_domains;
}

}  // namespace

// static
std::unique_ptr<net::SchemeHostPortMatcher>
UrlMatcherWithBypass::BuildBypassMatcher(
    const masked_domain_list::ResourceOwner& resource_owner) {
  auto bypass_matcher = std::make_unique<net::SchemeHostPortMatcher>();

  // De-dupe domains that are in owned_properties and owned_resources.
  std::set<std::string_view> domains;
  for (std::string_view property : resource_owner.owned_properties()) {
    domains.insert(property);
  }
  for (const masked_domain_list::Resource& resource :
       resource_owner.owned_resources()) {
    domains.insert(resource.domain());
  }

  for (std::string_view domain : domains) {
    AddRulesToMatcher(domain, !HasSubdomainCoverage(domain), *bypass_matcher);
  }

  return bypass_matcher;
}

// static
std::string UrlMatcherWithBypass::PartitionMapKey(std::string_view domain) {
  auto last_dot = domain.rfind(".");
  if (last_dot != std::string::npos) {
    auto penultimate_dot = domain.rfind(".", last_dot - 1);
    if (penultimate_dot != std::string::npos) {
      return std::string(domain.substr(penultimate_dot + 1));
    }
  }
  return std::string(domain);
}

// static
std::set<std::string> UrlMatcherWithBypass::GetEligibleDomains(
    const masked_domain_list::ResourceOwner& resource_owner,
    std::unordered_set<std::string> excluded_domains) {
  // Create a set of eligible domains.
  std::set<std::string> eligible_domains;
  std::transform(resource_owner.owned_resources().begin(),
                 resource_owner.owned_resources().end(),
                 std::inserter(eligible_domains, eligible_domains.begin()),
                 [](const Resource& resource) { return resource.domain(); });

  // If there are any excluded domains, remove them from the list of eligible
  // domains.
  if (!excluded_domains.empty()) {
    eligible_domains =
        ExcludeDomainsFromMDL(eligible_domains, excluded_domains);
  }

  return eligible_domains;
}

UrlMatcherWithBypass::UrlMatcherWithBypass() = default;
UrlMatcherWithBypass::~UrlMatcherWithBypass() = default;

void UrlMatcherWithBypass::AddRules(
    const masked_domain_list::ResourceOwner& resource_owner,
    const std::unordered_set<std::string>& excluded_domains,
    bool create_bypass_matcher) {
  net::SchemeHostPortMatcher* bypass_matcher = nullptr;

  // Extract eligble domains from resource_owner --> if 0 domains, exit early.
  std::set<std::string> eligible_domains =
      GetEligibleDomains(resource_owner, excluded_domains);
  if (eligible_domains.empty()) {
    return;
  }

  // Build the bypass matcher if requested by the caller.
  if (create_bypass_matcher) {
    bypass_matchers_.emplace_back(BuildBypassMatcher(resource_owner));
    bypass_matcher = bypass_matchers_.back().get();
  } else {
    bypass_matcher = &empty_bypass_matcher_;
  }

  // Add the eligible domains to the match_list_with_bypass_map_.
  for (const auto& [partition_key, partitioned_domains] :
       PartitionDomains(eligible_domains)) {
    net::SchemeHostPortMatcher matcher;
    for (const auto& domain : partitioned_domains) {
      DCHECK(domain.ends_with(partition_key));
      AddRulesToMatcher(domain, !HasSubdomainCoverage(domain), matcher);
    }

    if (!matcher.rules().empty()) {
      match_list_with_bypass_map_[partition_key].emplace_back(PartitionMatcher{
          .matcher = std::move(matcher), .bypass_matcher = bypass_matcher});
    }
  }
}

void UrlMatcherWithBypass::Clear() {
  match_list_with_bypass_map_.clear();
  bypass_matchers_.clear();
}

size_t UrlMatcherWithBypass::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(match_list_with_bypass_map_) +
         base::trace_event::EstimateMemoryUsage(bypass_matchers_);
}

bool UrlMatcherWithBypass::IsPopulated() const {
  return !match_list_with_bypass_map_.empty();
}

UrlMatcherWithBypassResult UrlMatcherWithBypass::Matches(
    const GURL& request_url,
    const std::optional<net::SchemefulSite>& top_frame_site,
    bool skip_bypass_check) const {
  auto vlog = [&](std::string_view message, bool matches) {
    VLOG(3) << "UrlMatcherWithBypass::Matches(" << request_url << ", "
            << top_frame_site.value() << ") - " << message
            << " - matches: " << (matches ? "true" : "false");
  };

  if (!skip_bypass_check && !top_frame_site.has_value()) {
    NOTREACHED()
        << "top frame site has no value and skip_bypass_check is false";
  }

  if (!IsPopulated()) {
    vlog("skipped (match list not populated)", false);
    return UrlMatcherWithBypassResult::kNoMatch;
  }

  std::string resource_host_suffix = PartitionMapKey(request_url.host());

  auto it = match_list_with_bypass_map_.find(resource_host_suffix);
  if (it == match_list_with_bypass_map_.end()) {
    vlog("no suffix match", false);
    return UrlMatcherWithBypassResult::kNoMatch;
  }

  for (const PartitionMatcher& partition_matcher : it->second) {
    auto rule_result = partition_matcher.matcher.Evaluate(request_url);
    if (rule_result == net::SchemeHostPortMatcherResult::kInclude) {
      if (skip_bypass_check) {
        vlog("matched with skipped bypass check", true);
        return UrlMatcherWithBypassResult::kMatchAndNoBypass;
      }
      const bool no_match = partition_matcher.bypass_matcher->Evaluate(
                                top_frame_site->GetURL()) ==
                            net::SchemeHostPortMatcherResult::kNoMatch;
      vlog("bypass_matcher.NoMatch", no_match);
      return no_match ? UrlMatcherWithBypassResult::kMatchAndNoBypass
                      : UrlMatcherWithBypassResult::kMatchAndBypass;
    }
  }

  vlog("no request match", false);
  return UrlMatcherWithBypassResult::kNoMatch;
}

}  // namespace ip_protection
