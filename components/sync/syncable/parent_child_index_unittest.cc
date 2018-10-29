// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/syncable/parent_child_index.h"

#include <string>

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/syncable/entry_kernel.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace syncable {

namespace {

const char kCacheGuid[] = "8HhNIHlEOCGQbIAALr9QEg==";

}  // namespace

class ParentChildIndexTest : public testing::Test {
 public:
  void TearDown() override {}

  // Unfortunately, we can't use the regular Entry factory methods, because the
  // ParentChildIndex deals in EntryKernels.

  static syncable::Id GetBookmarkRootId() {
    return syncable::Id::CreateFromServerId("bookmark_folder");
  }

  static syncable::Id GetBookmarkId(int n) {
    return syncable::Id::CreateFromServerId("b" + base::IntToString(n));
  }

  static syncable::Id GetClientUniqueId(int n) {
    return syncable::Id::CreateFromServerId("c" + base::IntToString(n));
  }

  EntryKernel* MakeRoot() {
    // Mimics the root node.
    EntryKernel* root = new EntryKernel();
    root->put(META_HANDLE, 1);
    root->put(BASE_VERSION, -1);
    root->put(SERVER_VERSION, 0);
    root->put(IS_DIR, true);
    root->put(ID, syncable::Id::GetRoot());
    root->put(PARENT_ID, syncable::Id::GetRoot());

    owned_entry_kernels_.push_back(base::WrapUnique(root));
    return root;
  }

  EntryKernel* MakeTypeRoot(ModelType model_type, const syncable::Id& id) {
    // Mimics a server-created bookmark folder.
    EntryKernel* folder = new EntryKernel;
    folder->put(META_HANDLE, 1);
    folder->put(BASE_VERSION, 9);
    folder->put(SERVER_VERSION, 9);
    folder->put(IS_DIR, true);
    folder->put(ID, id);
    folder->put(PARENT_ID, syncable::Id::GetRoot());
    folder->put(UNIQUE_SERVER_TAG, ModelTypeToRootTag(model_type));

    // Ensure that GetModelType() returns a correct value.
    sync_pb::EntitySpecifics specifics;
    AddDefaultFieldValue(model_type, &specifics);
    folder->put(SPECIFICS, specifics);

    owned_entry_kernels_.push_back(base::WrapUnique(folder));
    return folder;
  }

  EntryKernel* MakeBookmarkRoot() {
    return MakeTypeRoot(BOOKMARKS, GetBookmarkRootId());
  }

  EntryKernel* MakeBookmark(int n, int pos, bool is_dir) {
    // Mimics a regular bookmark or folder.
    EntryKernel* bm = new EntryKernel();
    bm->put(META_HANDLE, n);
    bm->put(BASE_VERSION, 10);
    bm->put(SERVER_VERSION, 10);
    bm->put(IS_DIR, is_dir);
    bm->put(ID, GetBookmarkId(n));
    bm->put(PARENT_ID, GetBookmarkRootId());

    bm->put(UNIQUE_BOOKMARK_TAG, GenerateSyncableBookmarkHash(
                                     kCacheGuid, bm->ref(ID).GetServerId()));

    UniquePosition unique_pos =
        UniquePosition::FromInt64(pos, bm->ref(UNIQUE_BOOKMARK_TAG));
    bm->put(UNIQUE_POSITION, unique_pos);
    bm->put(SERVER_UNIQUE_POSITION, unique_pos);

    owned_entry_kernels_.push_back(base::WrapUnique(bm));
    return bm;
  }

  EntryKernel* MakeUniqueClientItem(ModelType model_type,
                                    int n,
                                    const syncable::Id& parent_id) {
    EntryKernel* item = new EntryKernel();
    item->put(META_HANDLE, n);
    item->put(BASE_VERSION, 10);
    item->put(SERVER_VERSION, 10);
    item->put(IS_DIR, false);
    item->put(ID, GetClientUniqueId(n));
    item->put(UNIQUE_CLIENT_TAG, base::IntToString(n));

    if (!parent_id.IsNull()) {
      item->put(PARENT_ID, parent_id);
    }

    if (model_type != UNSPECIFIED) {
      // Ensure that GetModelType() returns a correct value.
      sync_pb::EntitySpecifics specifics;
      AddDefaultFieldValue(model_type, &specifics);
      item->put(SPECIFICS, specifics);
    }

    owned_entry_kernels_.push_back(base::WrapUnique(item));
    return item;
  }

  EntryKernel* MakeUniqueClientItem(int n, const syncable::Id& parent_id) {
    return MakeUniqueClientItem(UNSPECIFIED, n, parent_id);
  }

  EntryKernel* MakeUniqueClientItem(ModelType model_type, int n) {
    return MakeUniqueClientItem(model_type, n, Id());
  }

  const syncable::Id& IndexKnownModelTypeRootId(ModelType model_type) const {
    return index_.GetModelTypeRootId(model_type);
  }

  ParentChildIndex index_;

 private:
  std::vector<std::unique_ptr<EntryKernel>> owned_entry_kernels_;
};

TEST_F(ParentChildIndexTest, TestRootNode) {
  EntryKernel* root = MakeRoot();
  EXPECT_FALSE(ParentChildIndex::ShouldInclude(root));
}

TEST_F(ParentChildIndexTest, TestBookmarkRootFolder) {
  EntryKernel* bm_folder = MakeBookmarkRoot();
  EXPECT_TRUE(ParentChildIndex::ShouldInclude(bm_folder));

  index_.Insert(bm_folder);
  // Since BOOKMARKS is a hierarchical type, its type root folder shouldn't be
  // tracked by ParentChildIndex.
  EXPECT_EQ(Id(), IndexKnownModelTypeRootId(BOOKMARKS));
}

// Tests iteration over a set of siblings.
TEST_F(ParentChildIndexTest, ChildInsertionAndIteration) {
  EntryKernel* bm_folder = MakeBookmarkRoot();
  index_.Insert(bm_folder);

  // Make some folder and non-folder entries.
  EntryKernel* b1 = MakeBookmark(1, 1, false);
  EntryKernel* b2 = MakeBookmark(2, 2, false);
  EntryKernel* b3 = MakeBookmark(3, 3, true);
  EntryKernel* b4 = MakeBookmark(4, 4, false);

  // Insert them out-of-order to test different cases.
  index_.Insert(b3);  // Only child.
  index_.Insert(b4);  // Right-most child.
  index_.Insert(b1);  // Left-most child.
  index_.Insert(b2);  // Between existing items.

  // Double-check they've been added.
  EXPECT_TRUE(index_.Contains(b1));
  EXPECT_TRUE(index_.Contains(b2));
  EXPECT_TRUE(index_.Contains(b3));
  EXPECT_TRUE(index_.Contains(b4));

  // Check the ordering.
  const OrderedChildSet* children = index_.GetChildren(GetBookmarkRootId());
  ASSERT_TRUE(children);
  ASSERT_EQ(children->size(), 4UL);
  auto it = children->begin();
  EXPECT_EQ(*it, b1);
  it++;
  EXPECT_EQ(*it, b2);
  it++;
  EXPECT_EQ(*it, b3);
  it++;
  EXPECT_EQ(*it, b4);
  it++;
  EXPECT_TRUE(it == children->end());
}

// Tests iteration when hierarchy is involved.
TEST_F(ParentChildIndexTest, ChildInsertionAndIterationWithHierarchy) {
  EntryKernel* bm_folder = MakeBookmarkRoot();
  index_.Insert(bm_folder);

  // Just below the root, we have folders f1 and f2.
  EntryKernel* f1 = MakeBookmark(1, 1, false);
  EntryKernel* f2 = MakeBookmark(2, 2, false);
  EntryKernel* f3 = MakeBookmark(3, 3, false);

  // Under folder f1, we have two bookmarks.
  EntryKernel* f1_b1 = MakeBookmark(101, 1, false);
  f1_b1->put(PARENT_ID, GetBookmarkId(1));
  EntryKernel* f1_b2 = MakeBookmark(102, 2, false);
  f1_b2->put(PARENT_ID, GetBookmarkId(1));

  // Under folder f2, there is one bookmark.
  EntryKernel* f2_b1 = MakeBookmark(201, 1, false);
  f2_b1->put(PARENT_ID, GetBookmarkId(2));

  // Under folder f3, there is nothing.

  // Insert in a strange order, because we can.
  index_.Insert(f1_b2);
  index_.Insert(f2);
  index_.Insert(f2_b1);
  index_.Insert(f1);
  index_.Insert(f1_b1);
  index_.Insert(f3);

  OrderedChildSet::const_iterator it;

  // Iterate over children of the bookmark root.
  const OrderedChildSet* top_children = index_.GetChildren(GetBookmarkRootId());
  ASSERT_TRUE(top_children);
  ASSERT_EQ(top_children->size(), 3UL);
  it = top_children->begin();
  EXPECT_EQ(*it, f1);
  it++;
  EXPECT_EQ(*it, f2);
  it++;
  EXPECT_EQ(*it, f3);
  it++;
  EXPECT_TRUE(it == top_children->end());

  // Iterate over children of the first folder.
  const OrderedChildSet* f1_children = index_.GetChildren(GetBookmarkId(1));
  ASSERT_TRUE(f1_children);
  ASSERT_EQ(f1_children->size(), 2UL);
  it = f1_children->begin();
  EXPECT_EQ(*it, f1_b1);
  it++;
  EXPECT_EQ(*it, f1_b2);
  it++;
  EXPECT_TRUE(it == f1_children->end());

  // Iterate over children of the second folder.
  const OrderedChildSet* f2_children = index_.GetChildren(GetBookmarkId(2));
  ASSERT_TRUE(f2_children);
  ASSERT_EQ(f2_children->size(), 1UL);
  it = f2_children->begin();
  EXPECT_EQ(*it, f2_b1);
  it++;
  EXPECT_TRUE(it == f2_children->end());

  // Check for children of the third folder.
  const OrderedChildSet* f3_children = index_.GetChildren(GetBookmarkId(3));
  EXPECT_FALSE(f3_children);
}

// Tests removing items.
TEST_F(ParentChildIndexTest, RemoveWithHierarchy) {
  EntryKernel* bm_folder = MakeBookmarkRoot();
  index_.Insert(bm_folder);

  // Just below the root, we have folders f1 and f2.
  EntryKernel* f1 = MakeBookmark(1, 1, false);
  EntryKernel* f2 = MakeBookmark(2, 2, false);
  EntryKernel* f3 = MakeBookmark(3, 3, false);

  // Under folder f1, we have two bookmarks.
  EntryKernel* f1_b1 = MakeBookmark(101, 1, false);
  f1_b1->put(PARENT_ID, GetBookmarkId(1));
  EntryKernel* f1_b2 = MakeBookmark(102, 2, false);
  f1_b2->put(PARENT_ID, GetBookmarkId(1));

  // Under folder f2, there is one bookmark.
  EntryKernel* f2_b1 = MakeBookmark(201, 1, false);
  f2_b1->put(PARENT_ID, GetBookmarkId(2));

  // Under folder f3, there is nothing.

  // Insert in any order.
  index_.Insert(f2_b1);
  index_.Insert(f3);
  index_.Insert(f1_b2);
  index_.Insert(f1);
  index_.Insert(f2);
  index_.Insert(f1_b1);

  // Check that all are in the index.
  EXPECT_TRUE(index_.Contains(f1));
  EXPECT_TRUE(index_.Contains(f2));
  EXPECT_TRUE(index_.Contains(f3));
  EXPECT_TRUE(index_.Contains(f1_b1));
  EXPECT_TRUE(index_.Contains(f1_b2));
  EXPECT_TRUE(index_.Contains(f2_b1));

  // Remove them all in any order.
  index_.Remove(f3);
  EXPECT_FALSE(index_.Contains(f3));
  index_.Remove(f1_b2);
  EXPECT_FALSE(index_.Contains(f1_b2));
  index_.Remove(f2_b1);
  EXPECT_FALSE(index_.Contains(f2_b1));
  index_.Remove(f1);
  EXPECT_FALSE(index_.Contains(f1));
  index_.Remove(f2);
  EXPECT_FALSE(index_.Contains(f2));
  index_.Remove(f1_b1);
  EXPECT_FALSE(index_.Contains(f1_b1));
}

// Test that involves two non-ordered items.
TEST_F(ParentChildIndexTest, UnorderedChildren) {
  // Make two unique client tag items under the root node.
  EntryKernel* u1 = MakeUniqueClientItem(1, syncable::Id::GetRoot());
  EntryKernel* u2 = MakeUniqueClientItem(2, syncable::Id::GetRoot());

  EXPECT_FALSE(u1->ShouldMaintainPosition());
  EXPECT_FALSE(u2->ShouldMaintainPosition());

  index_.Insert(u1);
  index_.Insert(u2);

  const OrderedChildSet* children = index_.GetChildren(syncable::Id::GetRoot());
  EXPECT_EQ(children->count(u1), 1UL);
  EXPECT_EQ(children->count(u2), 1UL);
  EXPECT_EQ(children->size(), 2UL);
}

// Test ordered and non-ordered entries under the same parent.
// TODO(rlarocque): We should not need to support this.
TEST_F(ParentChildIndexTest, OrderedAndUnorderedChildren) {
  EntryKernel* bm_folder = MakeBookmarkRoot();
  index_.Insert(bm_folder);

  EntryKernel* b1 = MakeBookmark(1, 1, false);
  EntryKernel* b2 = MakeBookmark(2, 2, false);
  EntryKernel* u1 = MakeUniqueClientItem(1, GetBookmarkRootId());

  index_.Insert(b1);
  index_.Insert(u1);
  index_.Insert(b2);

  const OrderedChildSet* children = index_.GetChildren(GetBookmarkRootId());
  ASSERT_TRUE(children);
  EXPECT_EQ(3UL, children->size());

  // Ensure that the non-positionable item is moved to the far right.
  auto it = children->begin();
  EXPECT_EQ(*it, b1);
  it++;
  EXPECT_EQ(*it, b2);
  it++;
  EXPECT_EQ(*it, u1);
  it++;
  EXPECT_TRUE(it == children->end());
}

TEST_F(ParentChildIndexTest, NodesWithImplicitParentId) {
  syncable::Id type_root_id = syncable::Id::CreateFromServerId("type_root");
  EntryKernel* type_root = MakeTypeRoot(PREFERENCES, type_root_id);
  index_.Insert(type_root);
  EXPECT_EQ(type_root_id, IndexKnownModelTypeRootId(PREFERENCES));

  // Create entries without parent ID
  EntryKernel* p1 = MakeUniqueClientItem(PREFERENCES, 1);
  EntryKernel* p2 = MakeUniqueClientItem(PREFERENCES, 2);

  index_.Insert(p1);
  index_.Insert(p2);

  EXPECT_TRUE(index_.Contains(p1));
  EXPECT_TRUE(index_.Contains(p2));

  // Items should appear under the type root
  const OrderedChildSet* children = index_.GetChildren(type_root);
  ASSERT_TRUE(children);
  EXPECT_EQ(2UL, children->size());

  EXPECT_EQ(2UL, index_.GetSiblings(p1)->size());
  EXPECT_EQ(2UL, index_.GetSiblings(p2)->size());

  index_.Remove(p1);

  EXPECT_FALSE(index_.Contains(p1));
  EXPECT_TRUE(index_.Contains(p2));
  children = index_.GetChildren(type_root_id);
  ASSERT_TRUE(children);
  EXPECT_EQ(1UL, children->size());

  index_.Remove(p2);

  EXPECT_FALSE(index_.Contains(p2));
  children = index_.GetChildren(type_root);
  ASSERT_EQ(nullptr, children);
}

// Test that the removal isn't sensitive to the order (PurgeEntriesWithTypeIn
// removes items in arbitrary order).
TEST_F(ParentChildIndexTest, RemoveOutOfOrder) {
  // Insert a type root and two items (with implicit parent ID).
  syncable::Id type_root_id = syncable::Id::CreateFromServerId("type_root");
  EntryKernel* type_root = MakeTypeRoot(PREFERENCES, type_root_id);
  index_.Insert(type_root);
  EntryKernel* p1 = MakeUniqueClientItem(PREFERENCES, 1);
  EntryKernel* p2 = MakeUniqueClientItem(PREFERENCES, 2);
  index_.Insert(p1);
  index_.Insert(p2);

  // Two items expected under the type root.
  const OrderedChildSet* children = index_.GetChildren(type_root);
  ASSERT_TRUE(children);
  EXPECT_EQ(2UL, children->size());

  // Remove all 3 items in arbitrary order.
  index_.Remove(p2);
  index_.Remove(type_root);
  index_.Remove(p1);

  EXPECT_EQ(nullptr, index_.GetChildren(type_root));

  // Add a new root and another two items again.
  type_root = MakeTypeRoot(PREFERENCES, type_root_id);
  index_.Insert(type_root);

  index_.Insert(MakeUniqueClientItem(PREFERENCES, 3));
  index_.Insert(MakeUniqueClientItem(PREFERENCES, 4));

  children = index_.GetChildren(type_root);
  ASSERT_TRUE(children);
  // Should have 2 items. If the out of order removal cleared the implicit
  // parent folder ID prematurely, the collection would have 3 items including
  // p1.
  EXPECT_EQ(2UL, children->size());
}

// Test that the insert isn't sensitive to the order (Loading entries from
// Sync DB is done in arbitrary order).
TEST_F(ParentChildIndexTest, InsertOutOfOrder) {
  // Insert two Preferences entries with implicit parent first
  index_.Insert(MakeUniqueClientItem(PREFERENCES, 1));
  index_.Insert(MakeUniqueClientItem(PREFERENCES, 2));

  // Then insert the Preferences type root
  syncable::Id type_root_id = syncable::Id::CreateFromServerId("type_root");
  index_.Insert(MakeTypeRoot(PREFERENCES, type_root_id));

  // The index should still be able to associate Preferences entries
  // with the root.
  const OrderedChildSet* children = index_.GetChildren(type_root_id);
  ASSERT_TRUE(children);
  EXPECT_EQ(2UL, children->size());
}

// Test that if for some reason we wind up with multiple type roots, we
// gracefully handle it and don't allow any new entities to become invisible.
TEST_F(ParentChildIndexTest, MultipleTypeRoots) {
  // Create the good Preferences type root.
  syncable::Id type_root_id = syncable::Id::CreateFromClientString("type_root");
  index_.Insert(MakeTypeRoot(PREFERENCES, type_root_id));

  // Then insert the bad Preferences type root
  syncable::Id bad_type_root_id = syncable::Id::CreateFromServerId("bad");
  index_.Insert(MakeTypeRoot(PREFERENCES, bad_type_root_id));

  // Insert two Preferences entries with implicit parent.
  index_.Insert(MakeUniqueClientItem(PREFERENCES, 1));
  index_.Insert(MakeUniqueClientItem(PREFERENCES, 2));

  // The index should still be able to associate Preferences entries
  // with the good and bad roots.
  const OrderedChildSet* children = index_.GetChildren(type_root_id);
  ASSERT_TRUE(children);
  EXPECT_EQ(2UL, children->size());
  const OrderedChildSet* children_bad = index_.GetChildren(bad_type_root_id);
  ASSERT_TRUE(children_bad);
  EXPECT_EQ(2UL, children_bad->size());
}

// Test that if for some reason we wind up with multiple type roots, we
// gracefully handle it and don't allow any new entities to become invisible.
// Same as above but with the roots created in inverse order.
TEST_F(ParentChildIndexTest, MultipleTypeRootsInverse) {
  // Create the bad Preferences type root
  syncable::Id bad_type_root_id = syncable::Id::CreateFromServerId("bad");
  index_.Insert(MakeTypeRoot(PREFERENCES, bad_type_root_id));

  // Then insert the good Preferences type root.
  syncable::Id type_root_id = syncable::Id::CreateFromClientString("type_root");
  index_.Insert(MakeTypeRoot(PREFERENCES, type_root_id));

  // Insert two Preferences entries with implicit parent.
  index_.Insert(MakeUniqueClientItem(PREFERENCES, 1));
  index_.Insert(MakeUniqueClientItem(PREFERENCES, 2));

  // The index should still be able to associate Preferences entries
  // with the good root and bad roots.
  const OrderedChildSet* children = index_.GetChildren(type_root_id);
  ASSERT_TRUE(children);
  EXPECT_EQ(2UL, children->size());
  const OrderedChildSet* children_bad = index_.GetChildren(bad_type_root_id);
  ASSERT_TRUE(children_bad);
  EXPECT_EQ(2UL, children_bad->size());
}

}  // namespace syncable
}  // namespace syncer
