// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_MOCK_MOCK_BACKEND_H_
#define COMPONENTS_PERSISTENT_CACHE_MOCK_MOCK_BACKEND_H_

#include "components/persistent_cache/backend.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace persistent_cache {

class MockBackend : public Backend {
 public:
  MockBackend();
  ~MockBackend() override;

  MOCK_METHOD((base::expected<std::optional<EntryMetadata>, TransactionError>),
              Find,
              (std::string_view, BufferProvider buffer_provider),
              (override));
  MOCK_METHOD((base::expected<void, TransactionError>),
              Insert,
              (std::string_view key,
               base::span<const uint8_t> content,
               EntryMetadata metadata),
              (override));
  MOCK_METHOD(BackendType, GetType, (), (const, override));
  MOCK_METHOD(bool, IsReadOnly, (), (const, override));
  MOCK_METHOD(LockState, Abandon, (), (override));
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_MOCK_MOCK_BACKEND_H_
