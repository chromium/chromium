// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/syncable/read_node.h"

#include "base/logging.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/syncable/base_transaction.h"
#include "components/sync/syncable/entry.h"
#include "components/sync/syncable/syncable_base_transaction.h"

namespace syncer {

//////////////////////////////////////////////////////////////////////////
// ReadNode member definitions
ReadNode::ReadNode(const BaseTransaction* transaction)
    : entry_(nullptr), transaction_(transaction) {
  DCHECK(transaction);
}

ReadNode::ReadNode() {
  entry_ = nullptr;
  transaction_ = nullptr;
}

ReadNode::~ReadNode() {
  delete entry_;
}

void ReadNode::InitByRootLookup() {
  DCHECK(!entry_) << "Init called twice";
  syncable::BaseTransaction* trans = transaction_->GetWrappedTrans();
  entry_ = new syncable::Entry(trans, syncable::GET_BY_ID, trans->root_id());
  if (!entry_->good())
    DCHECK(false) << "Could not lookup root node for reading.";
}

BaseNode::InitByLookupResult ReadNode::InitByIdLookup(int64_t id) {
  DCHECK(!entry_) << "Init called twice";
  DCHECK_NE(id, kInvalidId);
  syncable::BaseTransaction* trans = transaction_->GetWrappedTrans();
  entry_ = new syncable::Entry(trans, syncable::GET_BY_HANDLE, id);
  if (!entry_->good())
    return INIT_FAILED_ENTRY_NOT_GOOD;
  if (entry_->GetIsDel())
    return INIT_FAILED_ENTRY_IS_DEL;
  ModelType model_type = GetModelType();
  LOG_IF(WARNING, model_type == UNSPECIFIED || model_type == TOP_LEVEL_FOLDER)
      << "SyncAPI InitByIdLookup referencing unusual object.";
  return DecryptIfNecessary() ? INIT_OK : INIT_FAILED_DECRYPT_IF_NECESSARY;
}

BaseNode::InitByLookupResult ReadNode::InitByClientTagLookup(
    ModelType model_type,
    const std::string& tag) {
  DCHECK(!entry_) << "Init called twice";
  if (tag.empty())
    return INIT_FAILED_PRECONDITION;

  const std::string hash = ClientTagHash::FromUnhashed(model_type, tag).value();

  entry_ = new syncable::Entry(transaction_->GetWrappedTrans(),
                               syncable::GET_BY_CLIENT_TAG, hash);
  if (!entry_->good())
    return INIT_FAILED_ENTRY_NOT_GOOD;
  if (entry_->GetIsDel())
    return INIT_FAILED_ENTRY_IS_DEL;
  return DecryptIfNecessary() ? INIT_OK : INIT_FAILED_DECRYPT_IF_NECESSARY;
}

const syncable::Entry* ReadNode::GetEntry() const {
  return entry_;
}

const BaseTransaction* ReadNode::GetTransaction() const {
  return transaction_;
}

int64_t ReadNode::GetTransactionVersion() const {
  return GetEntry()->GetTransactionVersion();
}

BaseNode::InitByLookupResult ReadNode::InitByTagLookupForBookmarks(
    const std::string& tag) {
  DCHECK(!entry_) << "Init called twice";
  if (tag.empty())
    return INIT_FAILED_PRECONDITION;
  syncable::BaseTransaction* trans = transaction_->GetWrappedTrans();
  entry_ = new syncable::Entry(trans, syncable::GET_BY_SERVER_TAG, tag);
  if (!entry_->good())
    return INIT_FAILED_ENTRY_NOT_GOOD;
  if (entry_->GetIsDel())
    return INIT_FAILED_ENTRY_IS_DEL;
  ModelType model_type = GetModelType();
  DCHECK_EQ(model_type, BOOKMARKS)
      << "InitByTagLookup deprecated for all types except bookmarks.";
  return DecryptIfNecessary() ? INIT_OK : INIT_FAILED_DECRYPT_IF_NECESSARY;
}

BaseNode::InitByLookupResult ReadNode::InitTypeRoot(ModelType type) {
  DCHECK(!entry_) << "Init called twice";
  if (!IsRealDataType(type))
    return INIT_FAILED_PRECONDITION;
  syncable::BaseTransaction* trans = transaction_->GetWrappedTrans();
  entry_ = new syncable::Entry(trans, syncable::GET_TYPE_ROOT, type);
  if (!entry_->good())
    return INIT_FAILED_ENTRY_NOT_GOOD;
  if (entry_->GetIsDel())
    return INIT_FAILED_ENTRY_IS_DEL;
  ModelType found_model_type = GetModelType();
  LOG_IF(WARNING, found_model_type == UNSPECIFIED ||
                      found_model_type == TOP_LEVEL_FOLDER)
      << "SyncAPI InitTypeRoot referencing unusually typed object.";
  return DecryptIfNecessary() ? INIT_OK : INIT_FAILED_DECRYPT_IF_NECESSARY;
}

}  // namespace syncer
