// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_TEST_SUPPORT_FAKE_DOM_STORAGE_DATABASE_FACTORY_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_TEST_SUPPORT_FAKE_DOM_STORAGE_DATABASE_FACTORY_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "components/services/storage/dom_storage/db_status.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/dom_storage/test_support/scoped_dom_storage_database_factory_for_testing.h"

namespace storage {

// A fake factory for creating FakeDomStorageDatabase instances in tests.
// The first `num_open_failures` Open() calls produce databases that return
// Corruption from Open(); subsequent calls produce databases that return OK.
//
// Destroying a pre-existing database is folded into Open() (via a non-empty
// `dir_to_destroy`). When an Open() destroys, its outcome is reported
// through `OpenResult::destroy_status`: the first `num_destroy_failures`
// destroys report IOError and subsequent destroys report OK, or, if a custom
// destroy-result callback is supplied, that callback supplies each outcome.
//
// Owns a ScopedDomStorageDatabaseFactoryForTesting internally, so the
// production factory is automatically overridden for the lifetime of this
// object.
class FakeDomStorageDatabaseFactory {
 public:
  // Supplies the result of a single destroy attempt. Invoked once per Open()
  // that destroys a pre-existing database, in order.
  using DestroyResultCallback = base::RepeatingCallback<DbStatus()>;

  FakeDomStorageDatabaseFactory(int num_open_failures,
                                int num_destroy_failures);

  // Overload with a custom destroy-result callback for tests that need
  // non-standard destroy behavior (e.g. first call succeeds, second fails).
  FakeDomStorageDatabaseFactory(int num_open_failures,
                                DestroyResultCallback custom_destroy_result);

  ~FakeDomStorageDatabaseFactory();

  FakeDomStorageDatabaseFactory(const FakeDomStorageDatabaseFactory&) = delete;
  FakeDomStorageDatabaseFactory& operator=(
      const FakeDomStorageDatabaseFactory&) = delete;

 private:
  void Open(StorageType storage_type,
            const base::FilePath& dir_to_open,
            const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
                memory_dump_id,
            const base::FilePath& dir_to_destroy,
            DomStorageDatabaseFactory::OpenResultCallback callback);

  // Returns the result of the next simulated destroy.
  DbStatus NextDestroyResult();

  const int num_open_failures_;
  const int num_destroy_failures_;
  const DestroyResultCallback custom_destroy_result_;
  int open_count_ = 0;
  int destroy_count_ = 0;

  // Must be declared last so it is destroyed first, ensuring the callbacks
  // referencing `this` are invalidated before the rest of this object.
  ScopedDomStorageDatabaseFactoryForTesting scoped_factory_;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_TEST_SUPPORT_FAKE_DOM_STORAGE_DATABASE_FACTORY_H_
