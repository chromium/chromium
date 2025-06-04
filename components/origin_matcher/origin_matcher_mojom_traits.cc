// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_matcher/origin_matcher_mojom_traits.h"

#include "base/strings/pattern.h"
#include "components/origin_matcher/origin_matcher_internal.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/parse_number.h"
#include "net/base/scheme_host_port_matcher_rule.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"
#include "url/url_util.h"

namespace mojo {

using origin_matcher::SubdomainMatchingRule;
using origin_matcher::mojom::OriginMatcherRuleDataView;

// static
origin_matcher::mojom::SubdomainMatchingRulePtr
StructTraits<OriginMatcherRuleDataView, OriginMatcherRuleUniquePtr>::
    subdomain_matching_rule(const OriginMatcherRuleUniquePtr& rule) {
  if (rule->type() == origin_matcher::OriginMatcherRuleType::kAny) {
    return nullptr;
  }

  DCHECK_EQ(origin_matcher::OriginMatcherRuleType::kSubdomain, rule->type());
  const SubdomainMatchingRule* matching_rule =
      static_cast<SubdomainMatchingRule*>(rule.get());
  origin_matcher::mojom::SubdomainMatchingRulePtr matching_rule_ptr(
      origin_matcher::mojom::SubdomainMatchingRule::New());

  matching_rule_ptr->scheme = matching_rule->scheme();
  matching_rule_ptr->optional_host = matching_rule->optional_host();
  matching_rule_ptr->optional_port = matching_rule->optional_port();
  return matching_rule_ptr;
}

// static
bool StructTraits<OriginMatcherRuleDataView, OriginMatcherRuleUniquePtr>::Read(
    OriginMatcherRuleDataView r,
    OriginMatcherRuleUniquePtr* out) {
  DCHECK(!out->get());

  origin_matcher::mojom::SubdomainMatchingRuleDataView
      subdomain_matching_rule_data_view;
  r.GetSubdomainMatchingRuleDataView(&subdomain_matching_rule_data_view);
  if (subdomain_matching_rule_data_view.is_null()) {
    *out = std::make_unique<origin_matcher::MatchAllOriginsRule>();
    return true;
  }

  origin_matcher::mojom::SubdomainMatchingRulePtr subdomain_matching_rule;
  if (!r.ReadSubdomainMatchingRule(&subdomain_matching_rule)) {
    return false;
  }
  if (!SubdomainMatchingRule::IsValidScheme(subdomain_matching_rule->scheme) ||
      !SubdomainMatchingRule::IsValidSchemeAndHost(
          subdomain_matching_rule->scheme,
          subdomain_matching_rule->optional_host)) {
    return false;
  }
  *out = std::make_unique<SubdomainMatchingRule>(
      subdomain_matching_rule->scheme, subdomain_matching_rule->optional_host,
      subdomain_matching_rule->optional_port);
  return true;
}

// static
bool StructTraits<origin_matcher::mojom::OriginMatcherDataView,
                  origin_matcher::OriginMatcher>::
    Read(origin_matcher::mojom::OriginMatcherDataView data,
         origin_matcher::OriginMatcher* out) {
  std::vector<OriginMatcherRuleUniquePtr> rules;
  if (!data.ReadRules(&rules)) {
    return false;
  }
  out->SetRules(std::move(rules));
  return true;
}

}  // namespace mojo
