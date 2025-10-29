// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_MOCK_MOCK_BACKEND_IMPL_H_
#define COMPONENTS_PERSISTENT_CACHE_MOCK_MOCK_BACKEND_IMPL_H_

#include <memory>
#include <optional>

#include "components/persistent_cache/backend.h"
#include "components/persistent_cache/backend_params.h"
#include "components/persistent_cache/entry.h"
#include "components/persistent_cache/entry_metadata.h"
#include "components/persistent_cache/transaction_error.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace persistent_cache {

class MockBackendImpl : public Backend {
 public:
  explicit MockBackendImpl(const BackendParams& backend_params);
  ~MockBackendImpl() override;
  BackendType GetType() const override;
  bool IsReadOnly() const override;

  MockBackendImpl(const MockBackendImpl&) = delete;
  MockBackendImpl(MockBackendImpl&&) = delete;
  MockBackendImpl& operator=(const MockBackendImpl&) = delete;
  MockBackendImpl& operator=(MockBackendImpl&&) = delete;

  // `Backend` overrides
  MOCK_METHOD(bool, Initialize, (), (override));
  MOCK_METHOD((base::expected<std::unique_ptr<Entry>, TransactionError>),
              Find,
              (std::string_view),
              (override));
  MOCK_METHOD((base::expected<void, TransactionError>),
              Insert,
              (std::string_view, base::span<const uint8_t>, EntryMetadata),
              (override));
  MOCK_METHOD(std::optional<BackendParams>,
              ExportReadOnlyParams,
              (),
              (override));
  MOCK_METHOD(std::optional<BackendParams>,
              ExportReadWriteParams,
              (),
              (override));
  MOCK_METHOD(void, Abandon, (), (override));
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_MOCK_MOCK_BACKEND_IMPL_H_
