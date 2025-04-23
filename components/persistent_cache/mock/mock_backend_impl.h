// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_MOCK_MOCK_BACKEND_IMPL_H_
#define COMPONENTS_PERSISTENT_CACHE_MOCK_MOCK_BACKEND_IMPL_H_

#include <memory>

#include "components/persistent_cache/backend.h"
#include "components/persistent_cache/entry.h"
#include "components/persistent_cache/entry_metadata.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace persistent_cache {

class MockBackendImpl : public Backend {
 public:
  explicit MockBackendImpl(const BackendParams& backend_params);
  ~MockBackendImpl() override;

  MockBackendImpl(const MockBackendImpl&) = delete;
  MockBackendImpl(MockBackendImpl&&) = delete;
  MockBackendImpl& operator=(const MockBackendImpl&) = delete;
  MockBackendImpl& operator=(MockBackendImpl&&) = delete;

  // `Backend` overrides
  MOCK_METHOD(bool, Initialize, (), (override));
  MOCK_METHOD(std::unique_ptr<Entry>, Find, (std::string_view), (override));
  MOCK_METHOD(void,
              Insert,
              (std::string_view, base::span<const uint8_t>, EntryMetadata),
              (override));
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_MOCK_MOCK_BACKEND_IMPL_H_
