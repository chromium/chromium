// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/js_injection/common/origin_matcher.h"

#include "base/containers/adapters.h"
#include "components/js_injection/common/origin_matcher_internal.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/parse_number.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"
#include "url/url_util.h"

namespace js_injection {

namespace {

inline int GetDefaultPortForSchemeIfNoPortInfo(const std::string& scheme,
                                               int port) {
  // The input has explicit port information, so don't modify it.
  if (port != -1)
    return port;

  // Hard code the port for http and https.
  if (scheme == url::kHttpScheme)
    return 80;
  if (scheme == url::kHttpsScheme)
    return 443;

  return port;
}

}  // namespace

OriginMatcher::OriginMatcher() = default;
// Allow copy and assign.
OriginMatcher::OriginMatcher(OriginMatcher&&) = default;
OriginMatcher& OriginMatcher::operator=(OriginMatcher&&) = default;

OriginMatcher::~OriginMatcher() = default;

OriginMatcher::OriginMatcher(const OriginMatcher& rhs) {
  *this = rhs;
}

OriginMatcher& OriginMatcher::operator=(const OriginMatcher& rhs) {
  rules_.clear();
  for (const auto& rule : rhs.Serialize())
    AddRuleFromString(rule);
  return *this;
}

void OriginMatcher::SetRules(RuleList rules) {
  rules_.swap(rules);
}

bool OriginMatcher::AddRuleFromString(const std::string& raw_untrimmed) {
  std::string raw;
  base::TrimWhitespaceASCII(raw_untrimmed, base::TRIM_ALL, &raw);

  if (raw == "*") {
    rules_.push_back(std::make_unique<MatchAllOriginsRule>());
    return true;
  }

  // Extract scheme-restriction.
  std::string::size_type scheme_pos = raw.find("://");
  if (scheme_pos == std::string::npos)
    return false;

  const std::string scheme = raw.substr(0, scheme_pos);
  if (!SubdomainMatchingRule::IsValidScheme(scheme))
    return false;

  std::string host_and_port = raw.substr(scheme_pos + 3);
  if (host_and_port.empty()) {
    if (!SubdomainMatchingRule::IsValidSchemeAndHost(scheme, std::string()))
      return false;
    rules_.push_back(
        std::make_unique<SubdomainMatchingRule>(scheme, std::string(), -1));
    return true;
  }

  std::string host;
  int port;
  if (!net::ParseHostAndPort(host_and_port, &host, &port) ||
      !SubdomainMatchingRule::IsValidSchemeAndHost(scheme, host)) {
    return false;
  }

  // Check if we have an <ip-address>[:port] input and try to canonicalize the
  // IP literal.
  net::IPAddress ip_address;
  if (ip_address.AssignFromIPLiteral(host)) {
    port = GetDefaultPortForSchemeIfNoPortInfo(scheme, port);
    host = ip_address.ToString();
    if (ip_address.IsIPv6())
      host = '[' + host + ']';
    rules_.push_back(
        std::make_unique<SubdomainMatchingRule>(scheme, host, port));
    return true;
  }

  port = GetDefaultPortForSchemeIfNoPortInfo(scheme, port);
  rules_.push_back(std::make_unique<SubdomainMatchingRule>(scheme, host, port));
  return true;
}

bool OriginMatcher::Matches(const url::Origin& origin) const {
  GURL origin_url = origin.GetURL();
  // Since we only do kInclude vs kNoMatch, the order doesn't actually matter.
  for (const std::unique_ptr<OriginMatcherRule>& rule :
       base::Reversed(rules_)) {
    net::SchemeHostPortMatcherResult result = rule->Evaluate(origin_url);
    if (result == net::SchemeHostPortMatcherResult::kInclude)
      return true;
  }
  return false;
}

std::vector<std::string> OriginMatcher::Serialize() const {
  std::vector<std::string> result;
  result.reserve(rules_.size());
  for (const auto& rule : rules_) {
    result.push_back(rule->ToString());
  }
  return result;
}

}  // namespace js_injection
