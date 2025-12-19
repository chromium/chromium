// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_TEST_SUPPORT_DOM_STORAGE_DATABASE_TESTING_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_TEST_SUPPORT_DOM_STORAGE_DATABASE_TESTING_H_

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

// All `DomStorageDatabase` implementations can share this test, which calls
// `DomStorageDatabase::UpdateMaps()` three times to:
// (1) Add key/value pairs to two maps.
// (2) Remove one key/value pair from the first map and two from the second map.
// (3) Clear all key/value pairs from the first map.
void TestUpdateMaps(DomStorageDatabase& database,
                    const DomStorageDatabase::MapLocator& map1_locator,
                    const DomStorageDatabase::MapLocator& map2_locator);

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
// `AsyncDomStorageDatabase::DeleteStorageKeysFromSession()`.  Expects success.
void DeleteStorageKeysFromSessionSync(
    AsyncDomStorageDatabase& database,
    std::string session_id,
    std::vector<blink::StorageKey> metadata_to_delete,
    std::vector<DomStorageDatabase::MapLocator> maps_to_delete);

// A synchronous wrapper for
// `AsyncDomStorageDatabase::DeleteSessions()`.  Expects success.
void DeleteSessionsSync(
    AsyncDomStorageDatabase& database,
    std::vector<std::string> session_ids,
    std::vector<DomStorageDatabase::MapLocator> maps_to_delete);

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_TEST_SUPPORT_DOM_STORAGE_DATABASE_TESTING_H_
