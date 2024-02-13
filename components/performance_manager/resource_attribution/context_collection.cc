// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/resource_attribution/context_collection.h"

#include <utility>

#include "base/containers/contains.h"

namespace resource_attribution {

using ResourceContextTypeId = internal::ResourceContextTypeId;

ContextCollection::ContextCollection() = default;

ContextCollection::~ContextCollection() = default;

ContextCollection::ContextCollection(const ContextCollection& other) = default;

ContextCollection& ContextCollection::operator=(
    const ContextCollection& other) = default;

void ContextCollection::AddResourceContext(const ResourceContext& context) {
  resource_contexts_.insert(context);
}

void ContextCollection::AddAllContextsOfType(ResourceContextTypeId type_id) {
  all_context_types_.set(type_id.value());
}

bool ContextCollection::IsEmpty() const {
  return resource_contexts_.empty() && all_context_types_.none();
}

bool ContextCollection::ContainsContext(const ResourceContext& context) const {
  return all_context_types_.test(ResourceContextTypeId(context).value()) ||
         base::Contains(resource_contexts_, context);
}

// static
ContextCollection ContextCollection::CreateForTesting(
    std::set<ResourceContext> resource_contexts,
    std::set<ResourceContextTypeId> all_context_types) {
  ContextCollection collection;
  collection.resource_contexts_ = std::move(resource_contexts);
  for (ResourceContextTypeId context_type : all_context_types) {
    collection.AddAllContextsOfType(context_type);
  }
  return collection;
}

}  // namespace resource_attribution
