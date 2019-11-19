// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/syncable/test_user_share.h"

#include <utility>

#include "base/compiler_specific.h"
#include "components/sync/syncable/directory.h"
#include "components/sync/syncable/directory_backing_store.h"
#include "components/sync/syncable/mutable_entry.h"
#include "components/sync/syncable/syncable_read_transaction.h"
#include "components/sync/syncable/syncable_write_transaction.h"
#include "components/sync/test/engine/test_directory_setter_upper.h"
#include "components/sync/test/engine/test_id_factory.h"
#include "components/sync/test/engine/test_syncable_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

TestUserShare::TestUserShare() : dir_maker_(new TestDirectorySetterUpper()) {}

TestUserShare::~TestUserShare() {
  if (user_share_)
    ADD_FAILURE() << "Should have called TestUserShare::TearDown()";
}

void TestUserShare::SetUp() {
  user_share_ = std::make_unique<UserShare>();
  dir_maker_->SetUp();

  // The pointer is owned by dir_maker_, we should not be storing it in a
  // unique_ptr.  We must be careful to ensure the unique_ptr never deletes it.
  user_share_->directory.reset(dir_maker_->directory());
}

void TestUserShare::TearDown() {
  // Ensure the unique_ptr doesn't delete the memory we don't own.
  ignore_result(user_share_->directory.release());

  user_share_.reset();
  dir_maker_->TearDown();
}

bool TestUserShare::Reload() {
  if (!user_share_->directory->SaveChanges())
    return false;

  std::unique_ptr<syncable::DirectoryBackingStore> saved_store =
      std::move(user_share_->directory->store_);

  // Ensure the unique_ptr doesn't delete the memory we don't own.
  ignore_result(user_share_->directory.release());
  user_share_ = std::make_unique<UserShare>();
  dir_maker_->SetUpWith(std::move(saved_store));
  user_share_->directory.reset(dir_maker_->directory());
  return true;
}

DirectoryCryptographer* TestUserShare::GetCryptographer(
    const syncable::BaseTransaction* trans) {
  return dir_maker_->GetCryptographer(trans);
}

UserShare* TestUserShare::user_share() {
  return user_share_.get();
}

SyncEncryptionHandler* TestUserShare::encryption_handler() {
  return dir_maker_->encryption_handler();
}

KeystoreKeysHandler* TestUserShare::keystore_keys_handler() {
  return dir_maker_->keystore_keys_handler();
}

syncable::TestTransactionObserver* TestUserShare::transaction_observer() {
  return dir_maker_->transaction_observer();
}

/* static */
bool TestUserShare::CreateRoot(ModelType model_type, UserShare* user_share) {
  syncable::Directory* directory = user_share->directory.get();
  syncable::WriteTransaction wtrans(FROM_HERE, syncable::UNITTEST, directory);
  CreateTypeRoot(&wtrans, directory, model_type);
  return true;
}

}  // namespace syncer
