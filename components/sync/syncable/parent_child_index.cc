// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/syncable/parent_child_index.h"

#include <utility>

#include "base/stl_util.h"
#include "components/sync/syncable/entry_kernel.h"

namespace syncer {
namespace syncable {

bool ChildComparator::operator()(const EntryKernel* a,
                                 const EntryKernel* b) const {
  const UniquePosition& a_pos = a->ref(UNIQUE_POSITION);
  const UniquePosition& b_pos = b->ref(UNIQUE_POSITION);

  if (a_pos.IsValid() && b_pos.IsValid()) {
    // Position is important to this type.
    return a_pos.LessThan(b_pos);
  } else if (a_pos.IsValid() && !b_pos.IsValid()) {
    // TODO(rlarocque): Remove this case.
    // An item with valid position as sibling of one with invalid position.
    // We should not support this, but the tests rely on it.  For now, just
    // move all invalid position items to the right.
    return true;
  } else if (!a_pos.IsValid() && b_pos.IsValid()) {
    // TODO(rlarocque): Remove this case.
    // Mirror of the above case.
    return false;
  } else {
    // Position doesn't matter.
    DCHECK(!a->ref(UNIQUE_POSITION).IsValid());
    DCHECK(!b->ref(UNIQUE_POSITION).IsValid());
    // Sort by META_HANDLE to ensure consistent order for testing.
    return a->ref(META_HANDLE) < b->ref(META_HANDLE);
  }
}

ParentChildIndex::ParentChildIndex() {
  // Pre-allocate these two vectors to the number of model types.
  model_type_root_ids_.resize(ModelType::NUM_ENTRIES);
  type_root_child_sets_.resize(ModelType::NUM_ENTRIES);
}

ParentChildIndex::~ParentChildIndex() {}

bool ParentChildIndex::ShouldInclude(const EntryKernel* entry) {
  // This index excludes deleted items and the root item.  The root
  // item is excluded so that it doesn't show up as a child of itself.
  return !entry->ref(IS_DEL) && !entry->ref(ID).IsRoot();
}

bool ParentChildIndex::Insert(EntryKernel* entry) {
  DCHECK(ShouldInclude(entry));

  OrderedChildSetRef siblings = nullptr;
  const Id& parent_id = entry->ref(PARENT_ID);
  ModelType model_type = entry->GetModelType();

  if (ShouldUseParentId(parent_id, model_type)) {
    // Hierarchical type, lookup child set in the map.
    DCHECK(!parent_id.IsNull());
    auto it = parent_children_map_.find(parent_id);
    if (it != parent_children_map_.end()) {
      siblings = it->second;
    } else {
      siblings = OrderedChildSetRef(new OrderedChildSet());
      parent_children_map_.insert(std::make_pair(parent_id, siblings));
    }
  } else {
    // Non-hierarchical type, return a pre-defined collection by type.
    siblings = GetOrCreateModelTypeChildSet(model_type);
  }

  // If this is one of type root folder for a non-hierarchical type, associate
  // its ID with the model type and the type's pre-defined child set with the
  // type root ID.
  // TODO(stanisc): crbug/438313: Just TypeSupportsHierarchy condition should
  // theoretically be sufficient but in practice many tests don't properly
  // initialize entries so TypeSupportsHierarchy ends up failing. Consider
  // tweaking TypeSupportsHierarchy and fixing all related test code.
  if (parent_id.IsRoot() && entry->ref(IS_DIR) && IsRealDataType(model_type) &&
      !TypeSupportsHierarchy(model_type)) {
    const Id& type_root_id = entry->ref(ID);

    // If the entry exists in the map it must already have the same
    // model type specific child set. It's possible another type root exists
    // in parent_children_map_, but that's okay as the new type root will
    // point to the same OrderedChildSet. As such, we just blindly store the
    // new type root ID and associate it to the (possibly existing) child set.
    model_type_root_ids_[model_type] = type_root_id;
    parent_children_map_.insert(
        std::make_pair(type_root_id, GetOrCreateModelTypeChildSet(model_type)));
  }

  // Finally, insert the entry in the child set.
  return siblings->insert(entry).second;
}

// Like the other containers used to help support the syncable::Directory, this
// one does not own any EntryKernels.  This function removes references to the
// given EntryKernel but does not delete it.
void ParentChildIndex::Remove(EntryKernel* e) {
  OrderedChildSetRef siblings = nullptr;
  ModelType model_type = e->GetModelType();
  const Id& parent_id = e->ref(PARENT_ID);
  bool should_erase = false;
  ParentChildrenMap::iterator sibling_iterator;

  if (ShouldUseParentId(parent_id, model_type)) {
    // Hierarchical type, lookup child set in the map.
    DCHECK(!parent_id.IsNull());
    sibling_iterator = parent_children_map_.find(parent_id);
    DCHECK(sibling_iterator != parent_children_map_.end());
    siblings = sibling_iterator->second;
    should_erase = true;
  } else {
    // Non-hierarchical type, return a pre-defined child set by type.
    siblings = type_root_child_sets_[model_type];
  }

  auto j = siblings->find(e);
  DCHECK(j != siblings->end());

  // Erase the entry from the child set.
  siblings->erase(j);
  // If the set is now empty and isn't shareable with |type_root_child_sets_|,
  // erase it from the map.
  if (siblings->empty() && should_erase) {
    parent_children_map_.erase(sibling_iterator);
  }
}

bool ParentChildIndex::Contains(EntryKernel* e) const {
  const OrderedChildSetRef siblings = GetChildSet(e);
  return siblings && siblings->count(e) > 0;
}

const OrderedChildSet* ParentChildIndex::GetChildren(const Id& id) const {
  DCHECK(!id.IsNull());

  auto parent = parent_children_map_.find(id);
  if (parent == parent_children_map_.end()) {
    return nullptr;
  }

  OrderedChildSetRef children = parent->second;
  // The expectation is that the function returns nullptr instead of an empty
  // child set
  if (children && children->empty())
    children = nullptr;
  return children.get();
}

const OrderedChildSet* ParentChildIndex::GetChildren(EntryKernel* e) const {
  return GetChildren(e->ref(ID));
}

const OrderedChildSet* ParentChildIndex::GetSiblings(EntryKernel* e) const {
  // This implies the entry is in the index.
  DCHECK(Contains(e));
  const OrderedChildSetRef siblings = GetChildSet(e);
  DCHECK(siblings && !siblings->empty());
  return siblings.get();
}

size_t ParentChildIndex::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  return EstimateMemoryUsage(parent_children_map_) +
         EstimateMemoryUsage(model_type_root_ids_) +
         EstimateMemoryUsage(type_root_child_sets_);
}

/* static */
bool ParentChildIndex::ShouldUseParentId(const Id& parent_id,
                                         ModelType model_type) {
  // For compatibility with legacy unit tests, in addition to hierarchical
  // entries, this returns true any entries directly under root and for entries
  // of UNSPECIFIED model type.
  return parent_id.IsRoot() || TypeSupportsHierarchy(model_type) ||
         !IsRealDataType(model_type);
}

const OrderedChildSetRef ParentChildIndex::GetChildSet(EntryKernel* e) const {
  ModelType model_type = e->GetModelType();

  const Id& parent_id = e->ref(PARENT_ID);
  if (ShouldUseParentId(parent_id, model_type)) {
    // Hierarchical type, lookup child set in the map.
    auto it = parent_children_map_.find(parent_id);
    if (it == parent_children_map_.end())
      return nullptr;
    return it->second;
  }

  // Non-hierarchical type, return a collection indexed by type.
  return GetModelTypeChildSet(model_type);
}

const OrderedChildSetRef ParentChildIndex::GetModelTypeChildSet(
    ModelType model_type) const {
  return type_root_child_sets_[model_type];
}

OrderedChildSetRef ParentChildIndex::GetOrCreateModelTypeChildSet(
    ModelType model_type) {
  if (!type_root_child_sets_[model_type])
    type_root_child_sets_[model_type] =
        OrderedChildSetRef(new OrderedChildSet());
  return type_root_child_sets_[model_type];
}

const Id& ParentChildIndex::GetModelTypeRootId(ModelType model_type) const {
  return model_type_root_ids_[model_type];
}

}  // namespace syncable
}  // namespace syncer
