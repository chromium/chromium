// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JS_INJECTION_COMMON_ORIGIN_MATCHER_INTERNAL_H_
#define COMPONENTS_JS_INJECTION_COMMON_ORIGIN_MATCHER_INTERNAL_H_

#include <string>

#include "net/base/scheme_host_port_matcher.h"

// NOTE: this file is an implementation detail and only used by code in
// js_injection.

namespace js_injection {

enum class OriginMatcherRuleType { kAny, kSubdomain };

// Common superclass that includes the type of matcher.
class OriginMatcherRule : public net::SchemeHostPortMatcherRule {
 public:
  explicit OriginMatcherRule(OriginMatcherRuleType type);
  ~OriginMatcherRule() override;

  OriginMatcherRuleType type() const { return type_; }

 private:
  const OriginMatcherRuleType type_;
};

// Matches *all* urls.
class MatchAllOriginsRule : public OriginMatcherRule {
 public:
  MatchAllOriginsRule();
  MatchAllOriginsRule(const MatchAllOriginsRule&) = delete;
  MatchAllOriginsRule& operator=(const MatchAllOriginsRule&) = delete;
  ~MatchAllOriginsRule() override;

  // OriginMatcherRule:
  net::SchemeHostPortMatcherResult Evaluate(const GURL& url) const override;
  std::string ToString() const override;
};

// Matches against a specific scheme, optional host (potentially with
// wild-cards) and an optional port.
class SubdomainMatchingRule : public OriginMatcherRule {
 public:
  SubdomainMatchingRule(const std::string& scheme,
                        const std::string& optional_host,
                        int optional_port);
  // This constructor is implemented only in tests, it does no checking of
  // args.
  SubdomainMatchingRule(const std::string& scheme,
                        const std::string& optional_host,
                        int optional_port,
                        bool for_test);
  SubdomainMatchingRule(const SubdomainMatchingRule&) = delete;
  SubdomainMatchingRule& operator=(const SubdomainMatchingRule&) = delete;
  ~SubdomainMatchingRule() override;

  // Returns true if |scheme| is a valid scheme identifier.
  static bool IsValidScheme(const std::string& scheme);

  // Returns true if the |scheme| is allowed to have a host and port part.
  static bool CanSchemeHaveHost(const std::string& scheme);

  // Returns true if |scheme| and |host| are valid.
  static bool IsValidSchemeAndHost(const std::string& scheme,
                                   const std::string& host);

  const std::string& scheme() const { return scheme_; }
  const std::string& optional_host() const { return optional_host_; }
  int optional_port() const { return optional_port_; }

  // OriginMatcherRule:
  net::SchemeHostPortMatcherResult Evaluate(const GURL& url) const override;
  std::string ToString() const override;

 private:
  const std::string scheme_;
  // Empty string means no host provided.
  const std::string optional_host_;
  // -1 means no port provided.
  const int optional_port_;
};

}  // namespace js_injection

#endif  // COMPONENTS_JS_INJECTION_COMMON_ORIGIN_MATCHER_INTERNAL_H_
