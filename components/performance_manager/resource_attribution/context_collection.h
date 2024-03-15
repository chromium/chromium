// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_CONTEXT_COLLECTION_H_
#define COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_CONTEXT_COLLECTION_H_

#include <bitset>
#include <set>

#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace resource_attribution {

// A mixed collection of individual ResourceContext's and
// ResourceContextTypeId's.
//
// ResourceContextTypeId's are integers that map to specific context types (ie.
// alternatives in the ResourceContext variant). In this collection they
// represent "all contexts of the given type", which is a set that changes over
// time as contexts are created and deleted.
class ContextCollection {
 public:
  ContextCollection();
  ~ContextCollection();

  ContextCollection(const ContextCollection& other);
  ContextCollection& operator=(const ContextCollection& other);

  friend bool operator==(const ContextCollection&,
                         const ContextCollection&) = default;

  // Adds `context` to the collection.
  void AddResourceContext(const ResourceContext& context);

  // Adds `type_id` to the collection so that all contexts of that type will be
  // implicitly included.
  void AddAllContextsOfType(internal::ResourceContextTypeId type_id);

  // Returns true iff the collection contains nothing.
  bool IsEmpty() const;

  // Returns true iff the collection contains `context`, either explicitly or
  // because the collection tracks all contexts of its type.
  bool ContainsContext(const ResourceContext& context) const;

  static ContextCollection CreateForTesting(
      std::set<ResourceContext> resource_contexts,
      std::set<internal::ResourceContextTypeId> all_context_types);

 private:
  // Individual resource contexts to measure.
  std::set<ResourceContext> resource_contexts_;

  // A set of ResourceContextTypeId's (converted to int and stored in a bitset
  // for efficiency). For each of these context types, all contexts that exist
  // will be measured.
  std::bitset<absl::variant_size<ResourceContext>::value> all_context_types_;
};

}  // namespace resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_CONTEXT_COLLECTION_H_
