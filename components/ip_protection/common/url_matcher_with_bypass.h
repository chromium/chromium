// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_URL_MATCHER_WITH_BYPASS_H_
#define COMPONENTS_IP_PROTECTION_COMMON_URL_MATCHER_WITH_BYPASS_H_

#include <map>
#include <memory>
#include <string_view>
#include <vector>

#include "base/types/optional_ref.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "net/base/scheme_host_port_matcher.h"
#include "net/base/scheme_host_port_matcher_result.h"
#include "net/base/scheme_host_port_matcher_rule.h"
#include "net/base/schemeful_site.h"
#include "url/gurl.h"

namespace ip_protection {

// The result of evaluating a URL in the UrlMatcherWithBypass.
enum class UrlMatcherWithBypassResult {
  // The URL is not matched to any MDL resource.
  kNoMatch,
  // The resource URL is owned by the requester and should bypass the proxy.
  kMatchAndBypass,
  // The URL matches a resource but it is not owned by the requester; should
  // proxy.
  kMatchAndNoBypass,
};

// This is a helper class for creating URL match lists for subresource request
// that can be bypassed with additional sets of rules based on the top frame
// URL.
class UrlMatcherWithBypass {
 public:
  UrlMatcherWithBypass();
  ~UrlMatcherWithBypass();

  // Returns true if there are entries in the match list and it is possible to
  // match on them. If false, `Matches` will always return false.
  bool IsPopulated() const;

  // Determines if the pair of URLs are a match by first trying to match on the
  // resource_url and then checking if the `top_frame_site` matches the bypass
  // match rules. If `skip_bypass_check` is true, the `top_frame_site` will not
  // be used to determine the outcome of the match.
  // `top_frame_site` should have a value if `skip_bypass_check` is false.
  UrlMatcherWithBypassResult Matches(
      const GURL& resource_url,
      const std::optional<net::SchemefulSite>& top_frame_site,
      bool skip_bypass_check = false) const;

  // Builds a pair of matcher and bypass rules for the each partition needed for
  // the set of domains. If a ResourceOwner is not provided then no bypass rules
  // will be created.
  void AddMaskedDomainListRules(
      const std::set<std::string>& domains,
      base::optional_ref<const masked_domain_list::ResourceOwner>
          resource_owner);

  // Builds a matcher to match to the public suffix list domains.
  void AddPublicSuffixListRules(const std::set<std::string>& domains);

  // Builds a matcher for each partition needed that does not have any bypass
  // rules.
  void AddRulesWithoutBypass(const std::set<std::string>& domains);

  void Clear();

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

  static std::unique_ptr<net::SchemeHostPortMatcher> BuildBypassMatcher(
      const masked_domain_list::ResourceOwner& resource_owner);

  // Determine the partition of the `match_list_with_bypass_map_` that contains
  // the given domain.
  static std::string PartitionMapKey(std::string_view domain);

 private:
  // Contains a single bypass matcher for each ResourceOwner that is referenced
  // by `match_list_with_bypass_map_`.
  // TODO(crbug.com/344506511): Remove this when `net::SchemeHostPortMatcher` is
  // replaced with a matcher that supports `scoped_refptr`.
  std::vector<std::unique_ptr<net::SchemeHostPortMatcher>> bypass_matchers_;

  // Empty matcher used by reference instead of creating new empty instances.
  net::SchemeHostPortMatcher empty_bypass_matcher_;

  // Maps partition map keys to smaller maps of domains eligible for the match
  // list and the top frame domains that allow the match list to be bypassed.
  std::map<std::string,
           std::vector<std::pair<net::SchemeHostPortMatcher,
                                 raw_ptr<net::SchemeHostPortMatcher>>>>
      match_list_with_bypass_map_;
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_URL_MATCHER_WITH_BYPASS_H_
