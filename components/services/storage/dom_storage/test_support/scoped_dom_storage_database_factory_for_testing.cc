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
    ~ScopedDomStorageDatabaseFactoryForTesting() {
  DomStorageDatabaseFactory::GetOpenCallback() =
      std::move(default_open_callback_);
}

}  // namespace storage
