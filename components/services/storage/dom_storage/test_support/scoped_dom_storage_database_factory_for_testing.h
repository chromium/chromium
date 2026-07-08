// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_TEST_SUPPORT_SCOPED_DOM_STORAGE_DATABASE_FACTORY_FOR_TESTING_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_TEST_SUPPORT_SCOPED_DOM_STORAGE_DATABASE_FACTORY_FOR_TESTING_H_

#include "components/services/storage/dom_storage/dom_storage_database.h"

namespace storage {

// Overrides DomStorageDatabaseFactory::Open() for the duration of this object's
// lifetime. Tests provide a callback that defines the behavior of Open(),
// including destroying a pre-existing database when `dir_to_destroy` is
// non-empty.
class ScopedDomStorageDatabaseFactoryForTesting {
 public:
  using OpenCallback = DomStorageDatabaseFactory::OpenCallback;

  explicit ScopedDomStorageDatabaseFactoryForTesting(
      OpenCallback open_callback);
  ~ScopedDomStorageDatabaseFactoryForTesting();

  ScopedDomStorageDatabaseFactoryForTesting(
      const ScopedDomStorageDatabaseFactoryForTesting&) = delete;
  ScopedDomStorageDatabaseFactoryForTesting& operator=(
      const ScopedDomStorageDatabaseFactoryForTesting&) = delete;

 private:
  OpenCallback default_open_callback_;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_TEST_SUPPORT_SCOPED_DOM_STORAGE_DATABASE_FACTORY_FOR_TESTING_H_
