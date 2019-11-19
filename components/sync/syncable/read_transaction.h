// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SYNCABLE_READ_TRANSACTION_H_
#define COMPONENTS_SYNC_SYNCABLE_READ_TRANSACTION_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/sync/syncable/base_transaction.h"

namespace base {
class Location;
}  // namespace base

namespace sync_pb {
class DataTypeContext;
}

namespace syncer {

struct UserShare;

// Sync API's ReadTransaction is a read-only BaseTransaction.  It wraps
// a syncable::ReadTransaction.
class ReadTransaction : public BaseTransaction {
 public:
  // Start a new read-only transaction on the specified repository.
  ReadTransaction(const base::Location& from_here, UserShare* share);

  // Resume the middle of a transaction. Will not close transaction.
  ReadTransaction(UserShare* share, syncable::BaseTransaction* trans);

  ~ReadTransaction() override;

  // BaseTransaction override.
  syncable::BaseTransaction* GetWrappedTrans() const override;

  // Return |transaction_version| of |type| stored in sync directory's
  // persisted info.
  int64_t GetModelVersion(ModelType type) const;

  // Fills |context| with the datatype context associated with |type|.
  void GetDataTypeContext(ModelType type,
                          sync_pb::DataTypeContext* context) const;

 private:
  void* operator new(size_t size);  // Transaction is meant for stack use only.

  // The underlying syncable object which this class wraps.
  syncable::BaseTransaction* transaction_;
  bool close_transaction_;

  DISALLOW_COPY_AND_ASSIGN(ReadTransaction);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SYNCABLE_READ_TRANSACTION_H_
