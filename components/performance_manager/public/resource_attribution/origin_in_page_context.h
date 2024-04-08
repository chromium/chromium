// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_ORIGIN_IN_PAGE_CONTEXT_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_ORIGIN_IN_PAGE_CONTEXT_H_

#include <compare>
#include <string>
#include <tuple>

#include "components/performance_manager/public/resource_attribution/page_context.h"
#include "url/origin.h"

namespace resource_attribution {

class OriginInPageContext {
 public:
  // Creates an OriginInPageContext covering all frames and workers in
  // `page_context` with the origin `origin`. Since the set of frames and
  // workers changes over time, the OriginInPageContext refers to an aggregate
  // of resource usage for a changing set of other contexts.
  OriginInPageContext(const url::Origin& origin,
                      const PageContext& page_context);
  ~OriginInPageContext();

  OriginInPageContext(const OriginInPageContext& other);
  OriginInPageContext& operator=(const OriginInPageContext& other);
  OriginInPageContext(OriginInPageContext&& other);
  OriginInPageContext& operator=(OriginInPageContext&& other);

  // Returns the origin this context covers.
  url::Origin GetOrigin() const;

  // Returns the PageContext this context is a subset of.
  PageContext GetPageContext() const;

  // Returns a string representation of the context for debugging. This matches
  // the interface of base::TokenType and base::UnguessableToken, for
  // convenience.
  std::string ToString() const;

  // Compare OriginInPageContexts by PageContext and origin.
  constexpr friend std::weak_ordering operator<=>(
      const OriginInPageContext& a,
      const OriginInPageContext& b) {
    // url::Origin doesn't define operator<=>.
    const auto a_tuple = std::tie(a.origin_, a.page_context_);
    const auto b_tuple = std::tie(b.origin_, b.page_context_);
    if (a_tuple < b_tuple) {
      return std::weak_ordering::less;
    }
    if (a_tuple == b_tuple) {
      return std::weak_ordering::equivalent;
    }
    return std::weak_ordering::greater;
  }

  // Test OriginInPageContexts for equality by PageContext and origin.
  constexpr friend bool operator==(const OriginInPageContext& a,
                                   const OriginInPageContext& b) {
    return a.origin_ == b.origin_ && a.page_context_ == b.page_context_;
  }

 private:
  url::Origin origin_;
  PageContext page_context_;
};

}  // namespace resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_ORIGIN_IN_PAGE_CONTEXT_H_
