// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/url_matcher_with_bypass.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "net/base/scheme_host_port_matcher.h"
#include "net/base/schemeful_site.h"
#include "url_matcher_with_bypass.h"

namespace ip_protection {

namespace {

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

UrlMatcherWithBypass::UrlMatcherWithBypass() = default;
UrlMatcherWithBypass::~UrlMatcherWithBypass() = default;

void UrlMatcherWithBypass::AddMaskedDomainListRules(
    const std::set<std::string>& domains,
    base::optional_ref<const masked_domain_list::ResourceOwner>
        resource_owner) {
  net::SchemeHostPortMatcher* bypass_matcher = nullptr;

  if (resource_owner.has_value()) {
    bypass_matchers_.emplace_back(BuildBypassMatcher(resource_owner.value()));
    bypass_matcher = bypass_matchers_.back().get();
  } else {
    bypass_matcher = &empty_bypass_matcher_;
  }

  for (const auto& [partition_key, partitioned_domains] :
       PartitionDomains(domains)) {
    net::SchemeHostPortMatcher matcher;
    for (const auto& domain : partitioned_domains) {
      DCHECK(domain.ends_with(partition_key));
      AddRulesToMatcher(domain, !HasSubdomainCoverage(domain), matcher);
    }

    if (!matcher.rules().empty()) {
      match_list_with_bypass_map_[partition_key].emplace_back(
          std::move(matcher), bypass_matcher);
    }
  }
}

void UrlMatcherWithBypass::AddRulesWithoutBypass(
    const std::set<std::string>& domains) {
  AddMaskedDomainListRules(domains, std::nullopt);
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

  if (!match_list_with_bypass_map_.contains(resource_host_suffix)) {
    vlog("no suffix match", false);
    return UrlMatcherWithBypassResult::kNoMatch;
  }

  for (const auto& [matcher, bypass_matcher] :
       match_list_with_bypass_map_.at(resource_host_suffix)) {
    auto rule_result = matcher.Evaluate(request_url);
    if (rule_result == net::SchemeHostPortMatcherResult::kInclude) {
      if (skip_bypass_check) {
        vlog("matched with skipped bypass check", true);
        return UrlMatcherWithBypassResult::kMatchAndNoBypass;
      }
      const bool no_match =
          bypass_matcher->Evaluate(top_frame_site->GetURL()) ==
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
