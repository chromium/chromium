// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_MATCHER_ORIGIN_MATCHER_MOJOM_TRAITS_H_
#define COMPONENTS_ORIGIN_MATCHER_ORIGIN_MATCHER_MOJOM_TRAITS_H_

#include <vector>

#include "components/origin_matcher/origin_matcher.h"
#include "components/origin_matcher/origin_matcher_internal.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

// List below origin_matcher_internal.h, so OriginMatcherRule's a complete type.
#include "components/origin_matcher/origin_matcher.mojom.h"

namespace mojo {

using OriginMatcherRuleUniquePtr =
    std::unique_ptr<origin_matcher::OriginMatcherRule>;

template <>
struct StructTraits<origin_matcher::mojom::OriginMatcherRuleDataView,
                    OriginMatcherRuleUniquePtr> {
  static origin_matcher::mojom::SubdomainMatchingRulePtr
  subdomain_matching_rule(const OriginMatcherRuleUniquePtr& rule);
  static bool Read(origin_matcher::mojom::OriginMatcherRuleDataView r,
                   OriginMatcherRuleUniquePtr* out);
};

template <>
struct StructTraits<origin_matcher::mojom::OriginMatcherDataView,
                    origin_matcher::OriginMatcher> {
 public:
  static const std::vector<OriginMatcherRuleUniquePtr>& rules(
      const origin_matcher::OriginMatcher& r) {
    return r.rules();
  }

  static bool Read(origin_matcher::mojom::OriginMatcherDataView data,
                   origin_matcher::OriginMatcher* out);
};

}  // namespace mojo

#endif  // COMPONENTS_ORIGIN_MATCHER_ORIGIN_MATCHER_MOJOM_TRAITS_H_
