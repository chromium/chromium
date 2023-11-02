// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JS_INJECTION_COMMON_ORIGIN_MATCHER_MOJOM_TRAITS_H_
#define COMPONENTS_JS_INJECTION_COMMON_ORIGIN_MATCHER_MOJOM_TRAITS_H_

#include <vector>

#include "components/js_injection/common/origin_matcher.h"
#include "components/js_injection/common/origin_matcher.mojom.h"
#include "components/js_injection/common/origin_matcher_internal.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

using OriginMatcherRuleUniquePtr =
    std::unique_ptr<js_injection::OriginMatcherRule>;

template <>
struct StructTraits<js_injection::mojom::OriginMatcherRuleDataView,
                    OriginMatcherRuleUniquePtr> {
  static js_injection::mojom::SubdomainMatchingRulePtr subdomain_matching_rule(
      const OriginMatcherRuleUniquePtr& rule);
  static bool Read(js_injection::mojom::OriginMatcherRuleDataView r,
                   OriginMatcherRuleUniquePtr* out);
};

template <>
struct StructTraits<js_injection::mojom::OriginMatcherDataView,
                    js_injection::OriginMatcher> {
 public:
  static const std::vector<OriginMatcherRuleUniquePtr>& rules(
      const js_injection::OriginMatcher& r) {
    return r.rules();
  }

  static bool Read(js_injection::mojom::OriginMatcherDataView data,
                   js_injection::OriginMatcher* out);
};

}  // namespace mojo

#endif  // COMPONENTS_JS_INJECTION_COMMON_ORIGIN_MATCHER_MOJOM_TRAITS_H_
