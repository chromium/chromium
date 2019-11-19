// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/syncable/read_transaction.h"

#include "components/sync/syncable/directory.h"
#include "components/sync/syncable/syncable_read_transaction.h"

namespace syncer {

//////////////////////////////////////////////////////////////////////////
// ReadTransaction member definitions
ReadTransaction::ReadTransaction(const base::Location& from_here,
                                 UserShare* share)
    : BaseTransaction(share), transaction_(nullptr), close_transaction_(true) {
  transaction_ =
      new syncable::ReadTransaction(from_here, share->directory.get());
}

ReadTransaction::ReadTransaction(UserShare* share,
                                 syncable::BaseTransaction* trans)
    : BaseTransaction(share), transaction_(trans), close_transaction_(false) {}

ReadTransaction::~ReadTransaction() {
  if (close_transaction_) {
    delete transaction_;
  }
}

syncable::BaseTransaction* ReadTransaction::GetWrappedTrans() const {
  return transaction_;
}

int64_t ReadTransaction::GetModelVersion(ModelType type) const {
  return transaction_->directory()->GetTransactionVersion(type);
}

void ReadTransaction::GetDataTypeContext(
    ModelType type,
    sync_pb::DataTypeContext* context) const {
  return transaction_->directory()->GetDataTypeContext(transaction_, type,
                                                       context);
}

}  // namespace syncer
