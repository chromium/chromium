// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_TEST_SUPPORT_FAKE_DOM_STORAGE_DATABASE_FACTORY_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_TEST_SUPPORT_FAKE_DOM_STORAGE_DATABASE_FACTORY_H_

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/dom_storage/test_support/scoped_dom_storage_database_factory_for_testing.h"

namespace storage {

// A fake factory for creating FakeDomStorageDatabase instances in tests.
// The first `num_open_failures` Create() calls produce databases that return
// Corruption from Open(); subsequent calls produce databases that return OK.
// The first `num_destroy_failures` Destroy() calls return IOError; subsequent
// calls return OK.
//
// Owns a ScopedDomStorageDatabaseFactoryForTesting internally, so the
// production factory is automatically overridden for the lifetime of this
// object.
class FakeDomStorageDatabaseFactory {
 public:
  FakeDomStorageDatabaseFactory(int num_open_failures,
                                int num_destroy_failures);

  // Overload with a custom destroy callback for tests that need non-standard
  // destroy behavior (e.g. first call succeeds, second fails).
  FakeDomStorageDatabaseFactory(
      int num_open_failures,
      DomStorageDatabaseFactory::DestroyCallback custom_destroy_callback);

  ~FakeDomStorageDatabaseFactory();

  FakeDomStorageDatabaseFactory(const FakeDomStorageDatabaseFactory&) = delete;
  FakeDomStorageDatabaseFactory& operator=(
      const FakeDomStorageDatabaseFactory&) = delete;

 private:
  base::SequenceBound<DomStorageDatabase> Create(
      StorageType storage_type,
      bool is_in_memory,
      scoped_refptr<base::SequencedTaskRunner> runner);

  void Destroy(const base::FilePath& database_path,
               DomStorageDatabaseFactory::StatusCallback callback);

  const int num_open_failures_;
  const int num_destroy_failures_;
  int create_count_ = 0;
  int destroy_count_ = 0;

  // Must be declared last so it is destroyed first, ensuring the callbacks
  // referencing `this` are invalidated before the rest of this object.
  ScopedDomStorageDatabaseFactoryForTesting scoped_factory_;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_TEST_SUPPORT_FAKE_DOM_STORAGE_DATABASE_FACTORY_H_
