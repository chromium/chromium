// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_ENGINE_TEST_DIRECTORY_SETTER_UPPER_H_
#define COMPONENTS_SYNC_TEST_ENGINE_TEST_DIRECTORY_SETTER_UPPER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/sync/base/test_unrecoverable_error_handler.h"
#include "components/sync/test/fake_sync_encryption_handler.h"
#include "components/sync/test/null_directory_change_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

namespace syncable {
class Directory;
class DirectoryBackingStore;
class TestTransactionObserver;
}

// A handy class that takes care of setting up and destroying a
// syncable::Directory instance for unit tests that require one.
//
// The expected usage is to make this a component of your test fixture:
//
//   class AwesomenessTest : public testing::Test {
//    public:
//     virtual void SetUp() {
//       metadb_.SetUp();
//     }
//     virtual void TearDown() {
//       metadb_.TearDown();
//     }
//    protected:
//     TestDirectorySetterUpper metadb_;
//   };
//
// Then, in your tests, get at the directory like so:
//
//   TEST_F(AwesomenessTest, IsMaximal) {
//     ... now use metadb_.directory() to get at syncable::Entry objects ...
//   }
//
class TestDirectorySetterUpper {
 public:
  TestDirectorySetterUpper();
  virtual ~TestDirectorySetterUpper();

  // Create a Directory instance open it.
  virtual void SetUp();

  // Create a Directory instance using |directory_store| as backend storage.
  // Takes ownership of |directory_store|.
  virtual void SetUpWith(
      std::unique_ptr<syncable::DirectoryBackingStore> directory_store);

  // Undo everything done by SetUp(): close the directory and delete the
  // backing files. Before closing the directory, this will run the directory
  // invariant checks and perform the SaveChanges action on the directory.
  virtual void TearDown();

  // Returns mutable version of Cryptographer owned by |encryption_handler_|.
  DirectoryCryptographer* GetCryptographer(
      const syncable::BaseTransaction* trans);

  syncable::Directory* directory() { return directory_.get(); }

  SyncEncryptionHandler* encryption_handler() { return &encryption_handler_; }

  KeystoreKeysHandler* keystore_keys_handler() { return &encryption_handler_; }

  syncable::TestTransactionObserver* transaction_observer() {
    return test_transaction_observer_.get();
  }

 private:
  syncable::NullDirectoryChangeDelegate delegate_;
  std::unique_ptr<syncable::TestTransactionObserver> test_transaction_observer_;
  TestUnrecoverableErrorHandler handler_;

  void RunInvariantCheck();

  FakeSyncEncryptionHandler encryption_handler_;
  std::unique_ptr<syncable::Directory> directory_;
  std::string name_;

  DISALLOW_COPY_AND_ASSIGN(TestDirectorySetterUpper);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_ENGINE_TEST_DIRECTORY_SETTER_UPPER_H_
