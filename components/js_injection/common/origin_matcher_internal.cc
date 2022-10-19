// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/js_injection/common/origin_matcher_internal.h"

#include "base/ranges/algorithm.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/scheme_host_port_matcher_rule.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace js_injection {
namespace {

// Returns false if |host| has too many wildcards.
inline bool HostWildcardSanityCheck(const std::string& host) {
  size_t wildcard_count = base::ranges::count(host, '*');
  if (wildcard_count == 0)
    return true;

  // We only allow one wildcard.
  if (wildcard_count > 1)
    return false;

  // Start with "*." for subdomain matching.
  if (base::StartsWith(host, "*.", base::CompareCase::SENSITIVE))
    return true;

  return false;
}

}  // namespace

OriginMatcherRule::OriginMatcherRule(OriginMatcherRuleType type)
    : type_(type) {}

OriginMatcherRule::~OriginMatcherRule() = default;

MatchAllOriginsRule::MatchAllOriginsRule()
    : OriginMatcherRule(OriginMatcherRuleType::kAny) {}

MatchAllOriginsRule::~MatchAllOriginsRule() = default;

net::SchemeHostPortMatcherResult MatchAllOriginsRule::Evaluate(
    const GURL& url) const {
  return net::SchemeHostPortMatcherResult::kInclude;
}

std::string MatchAllOriginsRule::ToString() const {
  return "*";
}

SubdomainMatchingRule::SubdomainMatchingRule(const std::string& scheme,
                                             const std::string& optional_host,
                                             int optional_port)
    : OriginMatcherRule(OriginMatcherRuleType::kSubdomain),
      scheme_(base::ToLowerASCII(scheme)),
      optional_host_(base::ToLowerASCII(optional_host)),
      optional_port_(optional_port) {
  DCHECK(IsValidScheme(scheme));
  DCHECK(IsValidSchemeAndHost(scheme_, optional_host_));
}

SubdomainMatchingRule::~SubdomainMatchingRule() = default;

// static
bool SubdomainMatchingRule::IsValidScheme(const std::string& scheme) {
  // Wild cards are not allowed in the scheme.
  return !scheme.empty() && scheme.find('*') == std::string::npos;
}

// static
bool SubdomainMatchingRule::CanSchemeHaveHost(const std::string& scheme) {
  return scheme == url::kHttpScheme || scheme == url::kHttpsScheme;
}

// static
bool SubdomainMatchingRule::IsValidSchemeAndHost(const std::string& scheme,
                                                 const std::string& host) {
  if (host.empty()) {
    if (CanSchemeHaveHost(scheme))
      return false;
    return true;
  }
  if (!CanSchemeHaveHost(scheme))
    return false;

  // |scheme| is either https or http.

  // URL like rule is invalid.
  if (host.find('/') != std::string::npos)
    return false;

  return HostWildcardSanityCheck(host);
}

net::SchemeHostPortMatcherResult SubdomainMatchingRule::Evaluate(
    const GURL& url) const {
  if (optional_port_ != -1 && url.EffectiveIntPort() != optional_port_) {
    // Didn't match port expectation.
    return net::SchemeHostPortMatcherResult::kNoMatch;
  }

  if (url.scheme() != scheme_) {
    // Didn't match scheme expectation.
    return net::SchemeHostPortMatcherResult::kNoMatch;
  }

  return base::MatchPattern(url.host(), optional_host_)
             ? net::SchemeHostPortMatcherResult::kInclude
             : net::SchemeHostPortMatcherResult::kNoMatch;
}

std::string SubdomainMatchingRule::ToString() const {
  std::string str;
  base::StringAppendF(&str, "%s://%s", scheme_.c_str(), optional_host_.c_str());
  if (optional_port_ != -1)
    base::StringAppendF(&str, ":%d", optional_port_);
  return str;
}

}  // namespace js_injection
