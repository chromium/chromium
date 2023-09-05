// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_RESOURCE_CONTEXTS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_RESOURCE_CONTEXTS_H_

#include "base/types/token_type.h"
#include "components/performance_manager/public/resource_attribution/type_helpers.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace performance_manager::resource_attribution {

// Each ResourceContext measured by resource attribution is identified by a
// token with the following properties:
//
// * Strongly typed, with a separate token type for each kind of context.
// * Within a type, each token value uniquely identifies a specific context.
// * Copyable (or ref counted) so that multiple token objects with the same
//   value can exist.
// * Values are not reused throughout the lifetime of the browser, so they can
//   continue to identify a context after it no longer exists.
// * Never null or invalid, although a token may not correspond to any existing
//   context. (For example if a token is allocated for an upcoming context, but
//   the expected context is never created.)
//
// ResourceContext is a variant that can hold all types of resource context
// tokens.
//
// ResourceContext tokens should never be passed to renderer processes, so that
// untrusted renderers can't use them to access contexts from other renderers.
//
// Implementation note: context tokens are implemented with `base::TokenType`
// because it conveniently has all the above properties, but this means they
// each contain a string. They could be replaced with a smaller representation
// if necessary as long as the above properties are maintained.

// Tokens for PerformanceManager nodes. There is one *Context type for each
// node type.
using FrameContext = TokenAlias<class FrameContextTag, blink::LocalFrameToken>;
using PageContext = base::TokenType<class PageContextTag>;
using ProcessContext = base::TokenType<class ProcessContextTag>;
using WorkerContext = TokenAlias<class WorkerContextTag, blink::WorkerToken>;

// A generic token representing any resource context.
//
// Implementation note: this doesn't use blink::MultiToken because it can only
// hold concrete instantiations of base::TokenType, not subclasses of it or
// nested MultiTokens.
using ResourceContext =
    absl::variant<FrameContext, PageContext, ProcessContext, WorkerContext>;

// Returns true iff `context` currently holds a resource context of type T.
template <typename T,
          internal::EnableIfIsVariantAlternative<T, ResourceContext> = true>
constexpr bool ContextIs(const ResourceContext& context) {
  return absl::holds_alternative<T>(context);
}

// If `context` currently holds a resource context of type T, returns a
// reference to that context. Otherwise, crashes.
template <typename T,
          internal::EnableIfIsVariantAlternative<T, ResourceContext> = true>
constexpr const T& AsContext(const ResourceContext& context) {
  return absl::get<T>(context);
}

// If `context` currently holds a resource context of type T, returns a
// copy of that context. Otherwise, returns nullopt.
template <typename T,
          internal::EnableIfIsVariantAlternative<T, ResourceContext> = true>
constexpr absl::optional<T> AsOptionalContext(const ResourceContext& context) {
  return internal::GetAsOptional<T>(context);
}

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_RESOURCE_CONTEXTS_H_
