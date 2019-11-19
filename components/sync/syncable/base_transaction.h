// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SYNCABLE_BASE_TRANSACTION_H_
#define COMPONENTS_SYNC_SYNCABLE_BASE_TRANSACTION_H_

#include "base/macros.h"
#include "components/sync/base/model_type.h"
#include "components/sync/nigori/cryptographer.h"
#include "components/sync/syncable/user_share.h"

namespace syncer {

enum class PassphraseType;

namespace syncable {
class BaseTransaction;
class Directory;
}

// Sync API's BaseTransaction, ReadTransaction, and WriteTransaction allow for
// batching of several read and/or write operations.  The read and write
// operations are performed by creating ReadNode and WriteNode instances using
// the transaction. These transaction classes wrap identically named classes in
// syncable, and are used in a similar way. Unlike syncable::BaseTransaction,
// whose construction requires an explicit syncable::Directory, a sync
// API BaseTransaction is created from a UserShare object.
//
// Note, these transactions are not atomic. Individual operations can
// fail. There is no built-in rollback or undo mechanism.
class BaseTransaction {
 public:
  // Provide access to the underlying syncable objects from BaseNode.
  virtual syncable::BaseTransaction* GetWrappedTrans() const = 0;
  const Cryptographer* GetCryptographer() const;
  ModelTypeSet GetEncryptedTypes() const;
  PassphraseType GetPassphraseType() const;

  syncable::Directory* GetDirectory() const {
    if (!user_share_) {
      return nullptr;
    } else {
      return user_share_->directory.get();
    }
  }

  UserShare* GetUserShare() const { return user_share_; }

 protected:
  explicit BaseTransaction(UserShare* share);
  virtual ~BaseTransaction();

  BaseTransaction() : user_share_(nullptr) {}

 private:
  UserShare* user_share_;

  DISALLOW_COPY_AND_ASSIGN(BaseTransaction);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SYNCABLE_BASE_TRANSACTION_H_
