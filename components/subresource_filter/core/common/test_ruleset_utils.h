// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_TEST_RULESET_UTILS_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_TEST_RULESET_UTILS_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/strings/string_piece.h"
#include "components/url_pattern_index/proto/rules.pb.h"

namespace subresource_filter {
namespace testing {

// Creates a blocklisted URL rule which targets subresources of any type with
// a URL containing the given `substring`.
url_pattern_index::proto::UrlRule CreateSubstringRule(
    base::StringPiece substring);

// Creates an allowlisted URL rule which targets subresources of any type with
// a URL containing the given `substring`.
url_pattern_index::proto::UrlRule CreateAllowlistSubstringRule(
    base::StringPiece substring);

// Creates a blocklisted URL rule which targets subresources of any type such
// that the resource URL ends with |suffix|.
url_pattern_index::proto::UrlRule CreateSuffixRule(base::StringPiece suffix);

// Creates an allowlisted URL rule which targets subresources of any type such
// that the resource URL ends with `suffix`. Note that a URL must match both an
// allowlist rule and a blocklist rule to be correctly considered allowlisted.
url_pattern_index::proto::UrlRule CreateAllowlistSuffixRule(
    base::StringPiece suffix);

// Creates a blocklisted URL rule which targets subresources of the specified
// `activation_types` and a URL containing the given `substring`. Additionally,
// it is restricted to a set of `initiator_domains`, if provided.
url_pattern_index::proto::UrlRule CreateRuleForDocument(
    base::StringPiece substring,
    int32_t activation_types =
        url_pattern_index::proto::ACTIVATION_TYPE_DOCUMENT,
    std::vector<std::string> initiator_domains = std::vector<std::string>());

// Creates an allowlisted URL rule which targets subresources of the specified
// `activation_types` and a URL containing the given `substring`. Additionally,
// it is restricted to a set of `initiator_domains`, if provided. Note that a
// URL must match both an allowlist rule and a blocklist rule to be correctly
// considered allowlisted.
url_pattern_index::proto::UrlRule CreateAllowlistRuleForDocument(
    base::StringPiece substring,
    int32_t activation_types =
        url_pattern_index::proto::ACTIVATION_TYPE_DOCUMENT,
    std::vector<std::string> initiator_domains = std::vector<std::string>());

}  // namespace testing
}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_TEST_RULESET_UTILS_H_
