// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_RESOURCE_CONTEXTS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_RESOURCE_CONTEXTS_H_

#include "components/performance_manager/public/resource_attribution/frame_context.h"
#include "components/performance_manager/public/resource_attribution/page_context.h"
#include "components/performance_manager/public/resource_attribution/process_context.h"
#include "components/performance_manager/public/resource_attribution/type_helpers.h"
#include "components/performance_manager/public/resource_attribution/worker_context.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace performance_manager::resource_attribution {

// Each ResourceContext measured by resource attribution is identified by an
// object with the following properties:
//
// * Strongly typed, with a separate type for each kind of context.
//   * eg. FrameContext corresponds to a DOM frame.
//
// * Instances contain all necessary information to identify and retrieve live
//   objects for a context, on either the UI thread or PM sequence.
//   * eg. FrameContext -> RenderFrameHost (UI) or FrameNode (PM).
//
// * Instances referring to the same context will always compare equal, even
//   after the live objects are deleted.
//
// * Strongly ordered, movable, and cheap to copy and store, so they can be used
//   efficiently as map keys.
//
// * Never null or invalid, although an instance may not correspond to any live
//   context.
//   * eg. after the context is destroyed, or if an instance is created for an
//     upcoming context, but the expected context is never created.
//
// ResourceContext is a variant that can hold all types of resource context
// objects.
//
// Data from a ResourceContext that can identify a renderer-specific context
// should never be passed to other renderers to prevent data leaks.

// Contexts for PerformanceManager nodes. There is one *Context type for each
// node type.

// A variant holding any type of resource context.
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
