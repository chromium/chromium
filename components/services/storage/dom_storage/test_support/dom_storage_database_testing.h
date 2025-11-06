// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_TEST_SUPPORT_DOM_STORAGE_DATABASE_TESTING_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_TEST_SUPPORT_DOM_STORAGE_DATABASE_TESTING_H_

#include <memory>

#include "base/containers/span.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"

namespace storage {
class AsyncDomStorageDatabase;

void ExpectEqualsMapLocator(const DomStorageDatabase::MapLocator& left,
                            const DomStorageDatabase::MapLocator& right);

void ExpectEqualsMapMetadata(const DomStorageDatabase::MapMetadata& left,
                             const DomStorageDatabase::MapMetadata& right);

void ExpectEqualsMapMetadataSpan(
    base::span<const DomStorageDatabase::MapMetadata> left,
    base::span<const DomStorageDatabase::MapMetadata> right);

std::vector<DomStorageDatabase::MapMetadata> CloneMapMetadata(
    base::span<const DomStorageDatabase::MapMetadata> source_span);

// A synchronous wrapper for
// `AsyncDomStorageDatabase::OpenInMemory()`.  Asserts success.
void OpenAsyncDomStorageDatabaseInMemorySync(
    StorageType storage_type,
    std::unique_ptr<AsyncDomStorageDatabase>* result);

// A synchronous wrapper for
// `AsyncDomStorageDatabase::ReadAllMetadata()`.  Expects success.
void ReadAllMetadataSync(AsyncDomStorageDatabase& database,
                         DomStorageDatabase::Metadata* metadata_results);

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_TEST_SUPPORT_DOM_STORAGE_DATABASE_TESTING_H_
