// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/test_support/scoped_dom_storage_database_factory_for_testing.h"

#include <utility>

namespace storage {

ScopedDomStorageDatabaseFactoryForTesting::
    ScopedDomStorageDatabaseFactoryForTesting(CreateCallback create_callback,
                                              DestroyCallback destroy_callback)
    : default_create_callback_(
          std::move(DomStorageDatabaseFactory::GetCreateCallback())),
      default_destroy_callback_(
          std::move(DomStorageDatabaseFactory::GetDestroyCallback())) {
  DomStorageDatabaseFactory::GetCreateCallback() = std::move(create_callback);
  DomStorageDatabaseFactory::GetDestroyCallback() = std::move(destroy_callback);
}

ScopedDomStorageDatabaseFactoryForTesting::
    ~ScopedDomStorageDatabaseFactoryForTesting() {
  DomStorageDatabaseFactory::GetCreateCallback() =
      std::move(default_create_callback_);
  DomStorageDatabaseFactory::GetDestroyCallback() =
      std::move(default_destroy_callback_);
}

}  // namespace storage
