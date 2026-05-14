// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_TEST_RULESET_UTILS_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_TEST_RULESET_UTILS_H_

#include <stdint.h>

#include <string>
#include <string_view>
#include <vector>

#include "components/url_pattern_index/proto/rules.pb.h"

namespace subresource_filter {
namespace testing {

// Creates a blocklisted URL rule which targets subresources of any type with
// a URL containing the given `substring`.
url_pattern_index::proto::UrlRule CreateSubstringRule(
    std::string_view substring);

// Creates an allowlisted URL rule which targets subresources of any type with
// a URL containing the given `substring`.
url_pattern_index::proto::UrlRule CreateAllowlistSubstringRule(
    std::string_view substring);

// Creates a blocklisted URL rule which targets subresources of any type such
// that the resource URL ends with |suffix|.
url_pattern_index::proto::UrlRule CreateSuffixRule(std::string_view suffix);

// Creates an allowlisted URL rule which targets subresources of any type such
// that the resource URL ends with `suffix`. Note that a URL must match both an
// allowlist rule and a blocklist rule to be correctly considered allowlisted.
url_pattern_index::proto::UrlRule CreateAllowlistSuffixRule(
    std::string_view suffix);

// Creates a blocklisted URL rule which targets subresources of the specified
// `activation_types` and a URL containing the given `substring`. Additionally,
// it is restricted to a set of `initiator_domains`, if provided.
url_pattern_index::proto::UrlRule CreateRuleForDocument(
    std::string_view substring,
    int32_t activation_types =
        url_pattern_index::proto::ACTIVATION_TYPE_DOCUMENT,
    std::vector<std::string> initiator_domains = std::vector<std::string>());

// Creates an allowlisted URL rule which targets subresources of the specified
// `activation_types` and a URL containing the given `substring`. Additionally,
// it is restricted to a set of `initiator_domains`, if provided. Note that a
// URL must match both an allowlist rule and a blocklist rule to be correctly
// considered allowlisted.
url_pattern_index::proto::UrlRule CreateAllowlistRuleForDocument(
    std::string_view substring,
    int32_t activation_types =
        url_pattern_index::proto::ACTIVATION_TYPE_DOCUMENT,
    std::vector<std::string> initiator_domains = std::vector<std::string>());

// Creates a StyleRule with the given |selector|, |domains|, |is_exclusion|,
// |classes|, and |ids|.
url_pattern_index::proto::StyleRule CreateStyleRule(
    std::string_view selector,
    const std::vector<std::string>& domains = {},
    bool is_exclusion = false,
    const std::vector<std::string>& classes = {},
    const std::vector<std::string>& ids = {});

struct StyleRuleParams {
  StyleRuleParams();
  ~StyleRuleParams();

  StyleRuleParams& SetSelector(std::string s) {
    selector = std::move(s);
    return *this;
  }
  StyleRuleParams& SetDomains(std::vector<std::string> d) {
    domains = std::move(d);
    return *this;
  }
  StyleRuleParams& SetExclusion(bool e) {
    is_exclusion = e;
    return *this;
  }
  StyleRuleParams& SetClasses(std::vector<std::string> c) {
    classes = std::move(c);
    return *this;
  }
  StyleRuleParams& SetIds(std::vector<std::string> i) {
    ids = std::move(i);
    return *this;
  }

  std::string selector;
  std::vector<std::string> domains;
  bool is_exclusion = false;
  std::vector<std::string> classes;
  std::vector<std::string> ids;
};

// Creates a StyleRule with the given `params`.
url_pattern_index::proto::StyleRule CreateStyleRule(
    const StyleRuleParams& params);

}  // namespace testing
}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_TEST_RULESET_UTILS_H_
