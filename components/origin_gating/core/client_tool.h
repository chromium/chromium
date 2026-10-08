// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_GATING_CORE_CLIENT_TOOL_H_
#define COMPONENTS_ORIGIN_GATING_CORE_CLIENT_TOOL_H_

#include <utility>

#include "base/check_op.h"
#include "base/memory/raw_ref.h"
#include "components/origin_gating/core/concepts.h"

namespace base {
class Value;
}  // namespace base

namespace origin_gating {

// Each enum type provides a singleton `kInstance<E>` value.
struct ToolDomain {
  template <IsIntCompatibleEnum E>
  static const ToolDomain kInstance;
};

// A type-safe, domain-attributed tool identifier. Decouples origin gating
// from embedder-specific tool enumerations.
class ClientTool {
 public:
  template <IsIntCompatibleEnum E>
  explicit constexpr ClientTool(E tool)
      : tool_(static_cast<int>(tool)), domain_(ToolDomain::kInstance<E>) {}

  friend bool operator==(const ClientTool&, const ClientTool&) = default;

  template <typename H>
  friend H AbslHashValue(H h, const ClientTool& tool) {
    return H::combine(std::move(h), tool.tool_, &tool.domain_.get());
  }

  bool IsSameDomain(const ClientTool& other) const {
    return &domain_.get() == &other.domain_.get();
  }

  template <IsIntCompatibleEnum E>
  E GetTool() const {
    CHECK_EQ(&domain_.get(), &ToolDomain::kInstance<E>);
    return static_cast<E>(tool_);
  }

  // Serializes `this` as a value for debugging.
  base::Value ToDebugValue() const;

 private:
  int tool_ = 0;
  raw_ref<const ToolDomain> domain_;
};

}  // namespace origin_gating

#endif  // COMPONENTS_ORIGIN_GATING_CORE_CLIENT_TOOL_H_
