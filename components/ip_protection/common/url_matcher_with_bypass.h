// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_URL_MATCHER_WITH_BYPASS_H_
#define COMPONENTS_IP_PROTECTION_COMMON_URL_MATCHER_WITH_BYPASS_H_

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "net/base/scheme_host_port_matcher.h"
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
      MdlType mdl_type,
      bool skip_bypass_check = false) const;

  // Builds a matcher to match to the public suffix list domains.
  void AddPublicSuffixListRules(const std::set<std::string>& domains);

  // Builds a matcher for each partition (per resource owner).
  //
  // `excluded_domains` is the set of domains that will be excluded from the
  // bypass matcher.
  // A bypass matcher will be created and paired to the partition if and only
  // if `create_bypass_matcher` is true.
  void AddRules(const masked_domain_list::ResourceOwner& resource_owner,
                const std::unordered_set<std::string>& excluded_domains,
                bool create_bypass_matcher);

  // Clears all url matchers within the object.
  void Clear();

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

  // Determine the partition of the `match_list_with_bypass_map_` that contains
  // the given domain.
  static std::string PartitionMapKey(std::string_view domain);

  // Returns the set of domains that are eligible for the experiment group.
  static std::set<std::string> GetEligibleDomains(
      const masked_domain_list::ResourceOwner& resource_owner,
      std::unordered_set<std::string> excluded_domains);

 private:
  struct PartitionMatcher {
    PartitionMatcher(
        std::vector<raw_ptr<net::SchemeHostPortMatcher>> matcher_ptrs,
        unsigned int bypass_matcher_key);

    ~PartitionMatcher();

    PartitionMatcher(const PartitionMatcher& other);

    std::vector<raw_ptr<net::SchemeHostPortMatcher>> matchers;

    // If this is set to 0, then the bypass matcher is not used. Otherwise, maps
    // to a vector of bypass matchers in `bypass_matchers_map_`.
    unsigned int bypass_matcher_key;

    // Estimates dynamic memory usage.
    // See base/trace_event/memory_usage_estimator.h for more info.
    size_t EstimateMemoryUsage() const;
  };

  // Maps bypass matcher keys to the bypass matchers. Each bypass matcher key in
  // partition matcher in `match_list_with_bypass_map_` references a vector of
  // bypass matchers in this map. The key 0 is reserved for matchers that are
  // not used as a bypass matcher.
  // NOTE: bypass_matchers_map_ must be declared before
  // match_list_with_bypass_map_ to ensure that the bypass matchers are
  // destroyed after the match list.
  std::map<unsigned int,
           std::vector<std::unique_ptr<net::SchemeHostPortMatcher>>>
      bypass_matchers_map_;

  // Maps the an experiment group to a map of the relevant map of partition
  // keys to a list of matchers.
  // This allows us to have a separate set of matchers for each experiment
  // group.
  std::map<std::pair<ip_protection::MdlType, std::string>,
           std::vector<PartitionMatcher>>
      match_list_with_bypass_map_;

  // This is used to generate a unique key for each bypass matcher. It MUST be
  // incremented for each resource owner that requires a bypass matcher to be
  // added AND set to 0 when `bypass_matchers_` is
  // cleared.
  // NOTE: 0 is reserved for matchers that do are not used as a bypass matcher.
  unsigned int bypass_matcher_key_ = 0;
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_URL_MATCHER_WITH_BYPASS_H_
