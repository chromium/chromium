// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/engine/test_syncable_utils.h"

#include "components/sync/syncable/directory.h"
#include "components/sync/syncable/entry.h"
#include "components/sync/syncable/mutable_entry.h"
#include "components/sync/syncable/syncable_base_transaction.h"
#include "components/sync/syncable/syncable_write_transaction.h"
#include "components/sync/test/engine/test_id_factory.h"

using std::string;

namespace syncer {
namespace syncable {

int CountEntriesWithName(BaseTransaction* rtrans,
                         const syncable::Id& parent_id,
                         const string& name) {
  Directory::Metahandles child_handles;
  rtrans->directory()->GetChildHandlesById(rtrans, parent_id, &child_handles);
  if (child_handles.size() <= 0) {
    return 0;
  }

  int number_of_entries_with_name = 0;
  for (auto i = child_handles.begin(); i != child_handles.end(); ++i) {
    Entry e(rtrans, GET_BY_HANDLE, *i);
    DCHECK(e.good());
    if (e.GetNonUniqueName() == name) {
      ++number_of_entries_with_name;
    }
  }
  return number_of_entries_with_name;
}

Id GetFirstEntryWithName(BaseTransaction* rtrans,
                         const syncable::Id& parent_id,
                         const string& name) {
  Directory::Metahandles child_handles;
  rtrans->directory()->GetChildHandlesById(rtrans, parent_id, &child_handles);

  for (auto i = child_handles.begin(); i != child_handles.end(); ++i) {
    Entry e(rtrans, GET_BY_HANDLE, *i);
    DCHECK(e.good());
    if (e.GetNonUniqueName() == name) {
      return e.GetId();
    }
  }

  DCHECK(false);
  return Id();
}

Id GetOnlyEntryWithName(BaseTransaction* rtrans,
                        const syncable::Id& parent_id,
                        const string& name) {
  DCHECK_EQ(1, CountEntriesWithName(rtrans, parent_id, name));
  return GetFirstEntryWithName(rtrans, parent_id, name);
}

void CreateTypeRoot(WriteTransaction* trans,
                    syncable::Directory* dir,
                    ModelType type) {
  // Root node for this type shouldn't exist yet.
  Entry same_type_root(trans, GET_TYPE_ROOT, type);
  DCHECK(!same_type_root.good());

  std::string tag_name = ModelTypeToRootTag(type);
  syncable::MutableEntry node(trans, syncable::CREATE, type,
                              TestIdFactory::root(), tag_name);
  DCHECK(node.good());
  node.PutUniqueServerTag(tag_name);
  node.PutIsDir(true);
  node.PutServerIsDir(false);
  node.PutIsUnsynced(false);
  node.PutIsUnappliedUpdate(false);
  node.PutServerVersion(20);
  node.PutBaseVersion(20);
  node.PutIsDel(false);
  node.PutId(TestIdFactory::MakeServer(tag_name));
  sync_pb::EntitySpecifics specifics;
  AddDefaultFieldValue(type, &specifics);
  node.PutServerSpecifics(specifics);
  node.PutSpecifics(specifics);
}

sync_pb::DataTypeProgressMarker BuildProgress(ModelType type) {
  sync_pb::DataTypeProgressMarker progress;
  progress.set_token("token");
  progress.set_data_type_id(GetSpecificsFieldNumberFromModelType(type));
  return progress;
}

}  // namespace syncable
}  // namespace syncer
