// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/test_support/scoped_dom_storage_database_factory_for_testing.h"

#include <utility>

namespace storage {

ScopedDomStorageDatabaseFactoryForTesting::
    ScopedDomStorageDatabaseFactoryForTesting(OpenCallback open_callback)
    : default_open_callback_(
          std::move(DomStorageDatabaseFactory::GetOpenCallback())) {
  DomStorageDatabaseFactory::GetOpenCallback() = std::move(open_callback);
}

ScopedDomStorageDatabaseFactoryForTesting::
    ScopedDomStorageDatabaseFactoryForTesting(
        MigrationCallback migration_callback)
    : default_migration_callback_(
          std::move(DomStorageDatabaseFactory::GetMigrationCallback())) {
  DomStorageDatabaseFactory::GetMigrationCallback() =
      std::move(migration_callback);
}

ScopedDomStorageDatabaseFactoryForTesting::
    ~ScopedDomStorageDatabaseFactoryForTesting() {
  if (default_open_callback_) {
    DomStorageDatabaseFactory::GetOpenCallback() =
        std::move(default_open_callback_);
  }

  if (default_migration_callback_) {
    DomStorageDatabaseFactory::GetMigrationCallback() =
        std::move(default_migration_callback_);
  }
}

}  // namespace storage
