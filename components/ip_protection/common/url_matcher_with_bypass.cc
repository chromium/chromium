// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/url_matcher_with_bypass.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/to_string.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "net/base/scheme_host_port_matcher.h"
#include "net/base/scheme_host_port_matcher_result.h"
#include "net/base/scheme_host_port_matcher_rule.h"
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
  // Extract eligible domains from resource_owner --> if 0 domains, exit early.
  std::set<std::string> eligible_domains =
      GetEligibleDomains(resource_owner, excluded_domains);
  if (eligible_domains.empty()) {
    return;
  }

  // Will contain a matcher for each domain in `resource_owner.owned_resources`
  // and `resource_owner.owned_properties`. The key is the domain.
  std::map<std::string, std::unique_ptr<net::SchemeHostPortMatcher>>
      matchers_map;

  // MDL types for each domain. This is set from
  // `resource_owner.owned_resources`. The key is the domain.
  std::map<std::string, std::vector<MdlType>> mdl_types_map;

  for (const auto& resource : resource_owner.owned_resources()) {
    std::string domain = resource.domain();

    mdl_types_map[domain] = FromMdlResourceProto(resource);

    auto matcher = std::make_unique<net::SchemeHostPortMatcher>();
    AddRulesToMatcher(domain, !HasSubdomainCoverage(domain), *matcher);
    matchers_map[domain] = std::move(matcher);
  }

  unsigned int bypass_matcher_key_to_use = 0;
  if (create_bypass_matcher) {
    bypass_matcher_key_++;
    bypass_matcher_key_to_use = bypass_matcher_key_;
    for (const auto& property : resource_owner.owned_properties()) {
      // If the property is already in the matchers map, then it is a domain
      // that is already in the owned_resources.
      if (base::Contains(matchers_map, property)) {
        continue;
      }

      auto matcher = std::make_unique<net::SchemeHostPortMatcher>();
      AddRulesToMatcher(property, !HasSubdomainCoverage(property), *matcher);
      matchers_map[property] = std::move(matcher);
    }
  }

  for (const auto& [partition_key, partitioned_domains] :
       PartitionDomains(eligible_domains)) {
    std::map<MdlType, std::vector<raw_ptr<net::SchemeHostPortMatcher>>>
        mdl_type_to_matcher_ptrs;
    for (const auto& domain : partitioned_domains) {
      DCHECK(domain.ends_with(partition_key));

      for (const auto& mdl_type : mdl_types_map[domain]) {
        mdl_type_to_matcher_ptrs[mdl_type].emplace_back(
            matchers_map[domain].get());
      }
    }

    for (auto& [mdl_type, matcher_ptrs] : mdl_type_to_matcher_ptrs) {
      std::pair<MdlType, std::string> key =
          std::make_pair(mdl_type, partition_key);
      matcher_ptrs.shrink_to_fit();
      match_list_with_bypass_map_[key].emplace_back(std::move(matcher_ptrs),
                                                    bypass_matcher_key_to_use);

      // Sort the matchers in a specific key by the matcher's longest domain
      // length.
      // TODO(crbug.com/389912816): Improve efficiency of this sorting using
      // sets.
      std::sort(
          match_list_with_bypass_map_[key].begin(),
          match_list_with_bypass_map_[key].end(),
          [](const PartitionMatcher& a, const PartitionMatcher& b) {
            return a.matchers.back()->rules().back()->ToString().length() >
                   b.matchers.back()->rules().back()->ToString().length();
          });
    }
  }

  // Collect the remaining matchers into the bypass matchers map to hold onto
  // them beyond the scope of this function.
  for (auto& [_, matcher] : matchers_map) {
    bypass_matchers_map_[bypass_matcher_key_to_use].emplace_back(
        std::move(matcher));
  }
}

void UrlMatcherWithBypass::Clear() {
  // `match_list_with_bypass_map_` must be cleared before `bypass_matchers_map_`
  // to ensure that there are no dangling pointers.
  match_list_with_bypass_map_.clear();
  bypass_matchers_map_.clear();
  bypass_matcher_key_ = 0;
}

size_t UrlMatcherWithBypass::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(bypass_matchers_map_) +
         base::trace_event::EstimateMemoryUsage(match_list_with_bypass_map_);
}

bool UrlMatcherWithBypass::IsPopulated() const {
  return !match_list_with_bypass_map_.empty();
}

UrlMatcherWithBypassResult UrlMatcherWithBypass::Matches(
    const GURL& request_url,
    const std::optional<net::SchemefulSite>& top_frame_site,
    MdlType mdl_type,
    bool skip_bypass_check) const {
  auto vlog = [&](std::string_view message, bool matches) {
    VLOG(3) << "UrlMatcherWithBypass::Matches(" << request_url << ", "
            << top_frame_site.value() << ") - " << message
            << " - matches: " << base::ToString(matches);
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

  auto it = match_list_with_bypass_map_.find(
      std::make_pair(mdl_type, resource_host_suffix));
  if (it == match_list_with_bypass_map_.end()) {
    vlog("no suffix match", false);
    return UrlMatcherWithBypassResult::kNoMatch;
  }

  for (const PartitionMatcher& partition_matcher : it->second) {
    for (const auto& matcher : partition_matcher.matchers) {
      auto rule_result = matcher->Evaluate(request_url);
      if (rule_result == net::SchemeHostPortMatcherResult::kInclude) {
        if (skip_bypass_check) {
          vlog("matched with skipped bypass check", true);
          return UrlMatcherWithBypassResult::kMatchAndNoBypass;
        }

        // Check bypass matchers.
        if (partition_matcher.bypass_matcher_key != 0) {
          for (const auto& bypass_matcher :
               bypass_matchers_map_.at(partition_matcher.bypass_matcher_key)) {
            if (bypass_matcher->Evaluate(top_frame_site->GetURL()) !=
                net::SchemeHostPortMatcherResult::kNoMatch) {
              vlog("bypass_matcher.NoMatch", false);
              return UrlMatcherWithBypassResult::kMatchAndBypass;
            }
          }
        }
        vlog("matched with no bypass", true);
        return UrlMatcherWithBypassResult::kMatchAndNoBypass;
      }
    }
  }

  vlog("no request match", false);
  return UrlMatcherWithBypassResult::kNoMatch;
}

// Define PartitionMatcher.
UrlMatcherWithBypass::PartitionMatcher::PartitionMatcher(
    std::vector<raw_ptr<net::SchemeHostPortMatcher>> matcher_ptrs,
    unsigned int bypass_matcher_key)
    : matchers(std::move(matcher_ptrs)),
      bypass_matcher_key(bypass_matcher_key) {}

UrlMatcherWithBypass::PartitionMatcher::~PartitionMatcher() = default;

UrlMatcherWithBypass::PartitionMatcher::PartitionMatcher(
    const PartitionMatcher& other) = default;

size_t UrlMatcherWithBypass::PartitionMatcher::EstimateMemoryUsage() const {
  return sizeof(matchers) +
         +(matchers.capacity() * sizeof(raw_ptr<net::SchemeHostPortMatcher>)) +
         sizeof(bypass_matcher_key);
}

}  // namespace ip_protection
