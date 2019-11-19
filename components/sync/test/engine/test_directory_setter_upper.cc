// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/engine/test_directory_setter_upper.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "components/sync/syncable/directory.h"
#include "components/sync/syncable/in_memory_directory_backing_store.h"
#include "components/sync/syncable/mutable_entry.h"
#include "components/sync/syncable/syncable_read_transaction.h"
#include "components/sync/syncable/syncable_write_transaction.h"
#include "components/sync/test/test_transaction_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

TestDirectorySetterUpper::TestDirectorySetterUpper() : name_("Test") {}

TestDirectorySetterUpper::~TestDirectorySetterUpper() {}

void TestDirectorySetterUpper::SetUp() {
  test_transaction_observer_ =
      std::make_unique<syncable::TestTransactionObserver>();
  WeakHandle<syncable::TransactionObserver> transaction_observer =
      MakeWeakHandle(test_transaction_observer_->AsWeakPtr());

  directory_ = std::make_unique<syncable::Directory>(
      std::make_unique<syncable::InMemoryDirectoryBackingStore>(
          name_, base::BindRepeating(
                     []() -> std::string { return "kTestCacheGuid"; })),
      MakeWeakHandle(handler_.GetWeakPtr()), base::Closure(),
      &encryption_handler_);
  ASSERT_EQ(syncable::OPENED_NEW,
            directory_->Open(name_, &delegate_, transaction_observer));
  directory_->set_cache_guid("kTestCacheGuid");
}

void TestDirectorySetterUpper::SetUpWith(
    std::unique_ptr<syncable::DirectoryBackingStore> directory_store) {
  DCHECK(directory_store);
  test_transaction_observer_ =
      std::make_unique<syncable::TestTransactionObserver>();
  WeakHandle<syncable::TransactionObserver> transaction_observer =
      MakeWeakHandle(test_transaction_observer_->AsWeakPtr());

  directory_ = std::make_unique<syncable::Directory>(
      std::move(directory_store), MakeWeakHandle(handler_.GetWeakPtr()),
      base::Closure(), &encryption_handler_);
  ASSERT_EQ(syncable::OPENED_EXISTING,
            directory_->Open(name_, &delegate_, transaction_observer));
  directory_->set_cache_guid("kTestCacheGuid");
}

void TestDirectorySetterUpper::TearDown() {
  if (!directory()->good())
    return;

  RunInvariantCheck();
  directory()->SaveChanges();
  RunInvariantCheck();
  directory()->SaveChanges();

  directory_.reset();
}

void TestDirectorySetterUpper::RunInvariantCheck() {
  // Check invariants for all items.
  syncable::ReadTransaction trans(FROM_HERE, directory());

  // The TestUnrecoverableErrorHandler that this directory was constructed with
  // will handle error reporting, so we can safely ignore the return value.
  directory()->FullyCheckTreeInvariants(&trans);
}

DirectoryCryptographer* TestDirectorySetterUpper::GetCryptographer(
    const syncable::BaseTransaction* trans) {
  DCHECK_EQ(directory_.get(), trans->directory());
  return encryption_handler_.GetMutableCryptographer();
}

}  // namespace syncer
