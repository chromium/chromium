// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/syncable/change_reorder_buffer.h"

#include <limits>
#include <set>
#include <utility>  // for pair<>

#include "base/containers/queue.h"
#include "components/sync/syncable/base_node.h"
#include "components/sync/syncable/base_transaction.h"
#include "components/sync/syncable/entry.h"
#include "components/sync/syncable/syncable_base_transaction.h"

using std::numeric_limits;
using std::pair;
using std::set;

namespace syncer {

// Traversal provides a way to collect a set of nodes from the syncable
// directory structure and then traverse them, along with any intermediate
// nodes, in a top-down fashion, starting from a single common ancestor.  A
// Traversal starts out empty and is grown by means of the ExpandToInclude
// method.  Once constructed, the top(), begin_children(), and end_children()
// methods can be used to explore the nodes in root-to-leaf order.
class ChangeReorderBuffer::Traversal {
 public:
  using ParentChildLink = pair<int64_t, int64_t>;
  using LinkSet = set<ParentChildLink>;

  Traversal() : top_(kInvalidId) {}

  // Expand the traversal so that it includes the node indicated by
  // |child_handle|.
  void ExpandToInclude(syncable::BaseTransaction* trans, int64_t child_handle) {
    // If |top_| is invalid, this is the first insertion -- easy.
    if (top_ == kInvalidId) {
      top_ = child_handle;
      return;
    }

    int64_t node_to_include = child_handle;
    while (node_to_include != kInvalidId && node_to_include != top_) {
      int64_t node_parent = 0;

      syncable::Entry node(trans, syncable::GET_BY_HANDLE, node_to_include);
      DCHECK(node.good());
      if (node.GetId().IsRoot()) {
        // If we've hit the root, and the root isn't already in the tree
        // (it would have to be |top_| if it were), start a new expansion
        // upwards from |top_| to unite the original traversal with the
        // path we just added that goes from |child_handle| to the root.
        node_to_include = top_;
        top_ = node.GetMetahandle();
      } else {
        // Otherwise, get the parent ID so that we can add a ParentChildLink.

        // Treat nodes with unset parent ID as if they were linked to the root.
        // That is a valid way to traverse the tree because all hierarchical
        // datatypes must have a valid parent ID and the ones with unset parent
        // ID have flat hierarchy where the order doesn't matter.
        const syncable::Id& parent_id = !node.GetParentId().IsNull()
                                            ? node.GetParentId()
                                            : syncable::Id::GetRoot();
        syncable::Entry parent(trans, syncable::GET_BY_ID, parent_id);
        DCHECK(parent.good());
        node_parent = parent.GetMetahandle();

        ParentChildLink link(node_parent, node_to_include);

        // If the link exists in the LinkSet |links_|, we don't need to search
        // any higher; we are done.
        if (links_.find(link) != links_.end())
          return;

        // Otherwise, extend |links_|, and repeat on the parent.
        links_.insert(link);
        node_to_include = node_parent;
      }
    }
  }

  // Return the top node of the traversal.  Use this as a starting point
  // for walking the tree.
  int64_t top() const { return top_; }

  // Return an iterator corresponding to the first child (in the traversal)
  // of the node specified by |parent_id|.  Iterate this return value until
  // it is equal to the value returned by end_children(parent_id).  The
  // enumeration thus provided is unordered.
  LinkSet::const_iterator begin_children(int64_t parent_id) const {
    return links_.upper_bound(
        ParentChildLink(parent_id, numeric_limits<int64_t>::min()));
  }

  // Return an iterator corresponding to the last child in the traversal
  // of the node specified by |parent_id|.
  LinkSet::const_iterator end_children(int64_t parent_id) const {
    return begin_children(parent_id + 1);
  }

 private:
  // The topmost point in the directory hierarchy that is in the traversal,
  // and thus the first node to be traversed.  If the traversal is empty,
  // this is kInvalidId.  If the traversal contains exactly one member, |top_|
  // will be the solitary member, and |links_| will be empty.
  int64_t top_;
  // A set of single-level links that compose the traversal below |top_|.  The
  // (parent, child) ordering of values enables efficient lookup of children
  // given the parent handle, which is used for top-down traversal.  |links_|
  // is expected to be connected -- every node that appears as a parent in a
  // link must either appear as a child of another link, or else be the
  // topmost node, |top_|.
  LinkSet links_;

  DISALLOW_COPY_AND_ASSIGN(Traversal);
};

ChangeReorderBuffer::ChangeReorderBuffer() {}

ChangeReorderBuffer::~ChangeReorderBuffer() {}

void ChangeReorderBuffer::PushAddedItem(int64_t id) {
  operations_[id] = ChangeRecord::ACTION_ADD;
}

void ChangeReorderBuffer::PushDeletedItem(int64_t id) {
  operations_[id] = ChangeRecord::ACTION_DELETE;
}

void ChangeReorderBuffer::PushUpdatedItem(int64_t id) {
  operations_[id] = ChangeRecord::ACTION_UPDATE;
}

void ChangeReorderBuffer::SetExtraDataForId(
    int64_t id,
    ExtraPasswordChangeRecordData* extra) {
  extra_data_[id] = make_linked_ptr<ExtraPasswordChangeRecordData>(extra);
}

void ChangeReorderBuffer::SetSpecificsForId(
    int64_t id,
    const sync_pb::EntitySpecifics& specifics) {
  specifics_[id] = specifics;
}

void ChangeReorderBuffer::Clear() {
  operations_.clear();
}

bool ChangeReorderBuffer::IsEmpty() const {
  return operations_.empty();
}

bool ChangeReorderBuffer::GetAllChangesInTreeOrder(
    const BaseTransaction* sync_trans,
    ImmutableChangeRecordList* changes) {
  syncable::BaseTransaction* trans = sync_trans->GetWrappedTrans();

  // Step 1: Iterate through the operations, doing three things:
  // (a) Push deleted items straight into the |changelist|.
  // (b) Construct a traversal spanning all non-deleted items.
  // (c) Construct a set of all parent nodes of any position changes.
  Traversal traversal;

  ChangeRecordList changelist;

  OperationMap::const_iterator i;
  for (i = operations_.begin(); i != operations_.end(); ++i) {
    if (i->second == ChangeRecord::ACTION_DELETE) {
      ChangeRecord record;
      record.id = i->first;
      record.action = i->second;
      if (specifics_.find(record.id) != specifics_.end())
        record.specifics = specifics_[record.id];
      if (extra_data_.find(record.id) != extra_data_.end())
        record.extra = extra_data_[record.id];
      changelist.push_back(record);
    } else {
      traversal.ExpandToInclude(trans, i->first);
    }
  }

  // Step 2: Breadth-first expansion of the traversal.
  base::queue<int64_t> to_visit;
  to_visit.push(traversal.top());
  while (!to_visit.empty()) {
    int64_t next = to_visit.front();
    to_visit.pop();

    // If the node has an associated action, output a change record.
    i = operations_.find(next);
    if (i != operations_.end()) {
      ChangeRecord record;
      record.id = next;
      record.action = i->second;
      if (specifics_.find(record.id) != specifics_.end())
        record.specifics = specifics_[record.id];
      if (extra_data_.find(record.id) != extra_data_.end())
        record.extra = extra_data_[record.id];
      changelist.push_back(record);
    }

    // Now add the children of |next| to |to_visit|.
    auto j = traversal.begin_children(next);
    auto end = traversal.end_children(next);
    for (; j != end; ++j) {
      DCHECK(j->first == next);
      to_visit.push(j->second);
    }
  }

  *changes = ImmutableChangeRecordList(&changelist);
  return true;
}

}  // namespace syncer
