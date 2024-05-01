// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_PATTERN_INDEX_URL_RULE_TEST_SUPPORT_H_
#define COMPONENTS_URL_PATTERN_INDEX_URL_RULE_TEST_SUPPORT_H_

#include <string>
#include <string_view>
#include <vector>

#include "components/url_pattern_index/proto/rules.pb.h"
#include "components/url_pattern_index/url_pattern.h"

class GURL;

namespace url {
class Origin;
}

namespace url_pattern_index {
namespace testing {

// Constants -------------------------------------------------------------------

constexpr proto::UrlPatternType kSubstring = proto::URL_PATTERN_TYPE_SUBSTRING;

constexpr proto::AnchorType kAnchorNone = proto::ANCHOR_TYPE_NONE;
constexpr proto::AnchorType kBoundary = proto::ANCHOR_TYPE_BOUNDARY;
constexpr proto::AnchorType kSubdomain = proto::ANCHOR_TYPE_SUBDOMAIN;

constexpr proto::ElementType kNoElement = proto::ELEMENT_TYPE_UNSPECIFIED;
constexpr proto::ElementType kOther = proto::ELEMENT_TYPE_OTHER;
constexpr proto::ElementType kScript = proto::ELEMENT_TYPE_SCRIPT;
constexpr proto::ElementType kImage = proto::ELEMENT_TYPE_IMAGE;
constexpr proto::ElementType kSubdocument = proto::ELEMENT_TYPE_SUBDOCUMENT;
constexpr proto::ElementType kFont = proto::ELEMENT_TYPE_FONT;
constexpr proto::ElementType kPopup = proto::ELEMENT_TYPE_POPUP;
constexpr proto::ElementType kWebSocket = proto::ELEMENT_TYPE_WEBSOCKET;
constexpr proto::ElementType kAllElementTypes = proto::ELEMENT_TYPE_ALL;

constexpr proto::ActivationType kNoActivation =
    proto::ACTIVATION_TYPE_UNSPECIFIED;
constexpr proto::ActivationType kDocument = proto::ACTIVATION_TYPE_DOCUMENT;
constexpr proto::ActivationType kGenericBlock =
    proto::ACTIVATION_TYPE_GENERICBLOCK;

constexpr proto::SourceType kAnyParty = proto::SOURCE_TYPE_ANY;
constexpr proto::SourceType kThirdParty = proto::SOURCE_TYPE_THIRD_PARTY;
constexpr proto::SourceType kFirstParty = proto::SOURCE_TYPE_FIRST_PARTY;

// Helpers ---------------------------------------------------------------------

// Creates a UrlRule with the given |url_pattern|, and all necessary fields
// initialized to defaults.
proto::UrlRule MakeUrlRule(const UrlPattern& url_pattern = UrlPattern());

// Parses `initiator_domains` and adds them to the initiator domain list of the
// `rule`.
//
// The `initiator_domains` vector should contain non-empty strings. If a string
// starts with '~' then the following part of the string is an exception domain.
void AddInitiatorDomains(const std::vector<std::string>& initiator_domains,
                         proto::UrlRule* rule);

// Parses `request_domains` and adds them to the request domain list of the
// `rule`. See `AddInitiatorDomains`.
void AddRequestDomains(const std::vector<std::string>& request_domains,
                       proto::UrlRule* rule);

// Returns the url::Origin parsed from |origin_string|, or the unique origin if
// the string is empty.
url::Origin GetOrigin(std::string_view origin_string);

// Returns whether |url| is third-party resource w.r.t. |first_party_origin|.
bool IsThirdParty(const GURL& url, const url::Origin& first_party_origin);

}  // namespace testing
}  // namespace url_pattern_index

#endif  // COMPONENTS_URL_PATTERN_INDEX_URL_RULE_TEST_SUPPORT_H_
