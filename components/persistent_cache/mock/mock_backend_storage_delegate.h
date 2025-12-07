// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_MOCK_MOCK_BACKEND_STORAGE_DELEGATE_H_
#define COMPONENTS_PERSISTENT_CACHE_MOCK_MOCK_BACKEND_STORAGE_DELEGATE_H_

#include "components/persistent_cache/backend.h"
#include "components/persistent_cache/backend_storage.h"
#include "components/persistent_cache/pending_backend.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace persistent_cache {

class MockBackendStorageDelegate : public BackendStorage::Delegate {
 public:
  MockBackendStorageDelegate();
  ~MockBackendStorageDelegate() override;

  MOCK_METHOD(std::optional<PendingBackend>,
              MakePendingBackend,
              (const base::FilePath& directory,
               const base::FilePath& base_name,
               bool single_connection,
               bool journal_mode_wal),
              (override));
  MOCK_METHOD(std::unique_ptr<Backend>,
              MakeBackend,
              (const base::FilePath& directory,
               const base::FilePath& base_name,
               bool single_connection,
               bool journal_mode_wal),
              (override));
  MOCK_METHOD(std::optional<PendingBackend>,
              ShareReadOnlyConnection,
              (const base::FilePath& directory,
               const base::FilePath& base_name,
               const Backend& backend),
              (override));
  MOCK_METHOD(std::optional<PendingBackend>,
              ShareReadWriteConnection,
              (const base::FilePath& directory,
               const base::FilePath& base_name,
               const Backend& backend),
              (override));
  MOCK_METHOD(base::FilePath,
              GetBaseName,
              (const base::FilePath& file),
              (override));
  MOCK_METHOD(int64_t,
              DeleteFiles,
              (const base::FilePath& directory,
               const base::FilePath& base_name),
              (override));
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_MOCK_MOCK_BACKEND_STORAGE_DELEGATE_H_
