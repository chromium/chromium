// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/syncable/entry.h"

#include <iomanip>

#include "base/values.h"
#include "components/sync/syncable/directory.h"
#include "components/sync/syncable/syncable_base_transaction.h"

namespace syncer {
namespace syncable {

Entry::Entry(BaseTransaction* trans, GetById, const Id& id)
    : basetrans_(trans) {
  kernel_ = trans->directory()->GetEntryById(id);
}

Entry::Entry(BaseTransaction* trans, GetByClientTag, const std::string& tag)
    : basetrans_(trans) {
  kernel_ = trans->directory()->GetEntryByClientTag(tag);
}

Entry::Entry(BaseTransaction* trans, GetTypeRoot, ModelType type)
    : basetrans_(trans) {
  const std::string& tag = ModelTypeToRootTag(type);
  kernel_ = trans->directory()->GetEntryByServerTag(tag);
}

Entry::Entry(BaseTransaction* trans, GetByHandle, int64_t metahandle)
    : basetrans_(trans) {
  kernel_ = trans->directory()->GetEntryByHandle(metahandle);
}

Entry::Entry(BaseTransaction* trans, GetByServerTag, const std::string& tag)
    : basetrans_(trans) {
  kernel_ = trans->directory()->GetEntryByServerTag(tag);
}

Directory* Entry::dir() const {
  return basetrans_->directory();
}

std::unique_ptr<base::DictionaryValue> Entry::ToValue(
    const Cryptographer* cryptographer) const {
  auto entry_info = std::make_unique<base::DictionaryValue>();
  entry_info->SetBoolean("good", good());
  if (good()) {
    entry_info->Set("kernel", kernel_->ToValue(cryptographer));
    entry_info->Set("modelType", ModelTypeToValue(GetModelType()));
    entry_info->SetBoolean("existsOnClientBecauseNameIsNonEmpty",
                           ExistsOnClientBecauseNameIsNonEmpty());
    entry_info->SetBoolean("isRoot", IsRoot());
  }
  return entry_info;
}

bool Entry::GetSyncing() const {
  DCHECK(kernel_);
  return kernel_->ref(SYNCING);
}

bool Entry::GetDirtySync() const {
  DCHECK(kernel_);
  return kernel_->ref(DIRTY_SYNC);
}

ModelType Entry::GetServerModelType() const {
  ModelType specifics_type = kernel_->GetServerModelType();
  if (specifics_type != UNSPECIFIED)
    return specifics_type;

  // Otherwise, we don't have a server type yet.  That should only happen
  // if the item is an uncommitted locally created item.
  // It's possible we'll need to relax these checks in the future; they're
  // just here for now as a safety measure.
  DCHECK(GetIsUnsynced());
  DCHECK_EQ(GetServerVersion(), 0);
  DCHECK(GetServerIsDel());
  // Note: can't enforce !GetId().ServerKnows() here because that could
  // actually happen if we hit AttemptReuniteLostCommitResponses.
  return UNSPECIFIED;
}

ModelType Entry::GetModelType() const {
  ModelType specifics_type = GetModelTypeFromSpecifics(GetSpecifics());
  if (specifics_type != UNSPECIFIED)
    return specifics_type;
  if (IsRoot())
    return TOP_LEVEL_FOLDER;
  // Loose check for server-created top-level folders that aren't
  // bound to a particular model type.
  if (!GetUniqueServerTag().empty() && GetIsDir())
    return TOP_LEVEL_FOLDER;

  return UNSPECIFIED;
}

Id Entry::GetPredecessorId() const {
  return dir()->GetPredecessorId(kernel_);
}

Id Entry::GetSuccessorId() const {
  return dir()->GetSuccessorId(kernel_);
}

Id Entry::GetFirstChildId() const {
  return dir()->GetFirstChildId(basetrans_, kernel_);
}

void Entry::GetChildHandles(std::vector<int64_t>* result) const {
  dir()->GetChildHandlesById(basetrans_, GetId(), result);
}

int Entry::GetTotalNodeCount() const {
  return dir()->GetTotalNodeCount(basetrans_, kernel_);
}

int Entry::GetPositionIndex() const {
  return dir()->GetPositionIndex(basetrans_, kernel_);
}

bool Entry::ShouldMaintainPosition() const {
  return kernel_->ShouldMaintainPosition();
}

bool Entry::ShouldMaintainHierarchy() const {
  return kernel_->ShouldMaintainHierarchy();
}

std::ostream& operator<<(std::ostream& os, const Entry& entry) {
  os << *(entry.kernel_);
  return os;
}

}  // namespace syncable
}  // namespace syncer
