// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_TEST_SUPPORT_DOM_STORAGE_DATABASE_TESTING_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_TEST_SUPPORT_DOM_STORAGE_DATABASE_TESTING_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/dom_storage/session_storage_metadata.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {
class AsyncDomStorageDatabase;

void ExpectEqualsMapLocator(const DomStorageDatabase::MapLocator& left,
                            const DomStorageDatabase::MapLocator& right);

void ExpectEqualsMapMetadata(const DomStorageDatabase::MapMetadata& left,
                             const DomStorageDatabase::MapMetadata& right);

void ExpectEqualsMapMetadataSpan(
    base::span<const DomStorageDatabase::MapMetadata> left,
    base::span<const DomStorageDatabase::MapMetadata> right);

DomStorageDatabase::MapMetadata CloneMapMetadata(
    const DomStorageDatabase::MapMetadata& source);

std::vector<DomStorageDatabase::MapMetadata> CloneMapMetadataVector(
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

// A synchronous wrapper for
// `AsyncDomStorageDatabase::PutMetadata()`.  Asserts success.
void PutMetadataSync(AsyncDomStorageDatabase& database,
                     DomStorageDatabase::Metadata metadata);

// A synchronous wrapper for
// `AsyncDomStorageDatabase::DeleteStorageKeysFromSessionSync()`.  Expects
// success.
void DeleteStorageKeysFromSessionSync(
    AsyncDomStorageDatabase& database,
    std::string session_id,
    std::vector<blink::StorageKey> storage_keys,
    absl::flat_hash_set<int64_t> excluded_cloned_map_ids);

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_TEST_SUPPORT_DOM_STORAGE_DATABASE_TESTING_H_
