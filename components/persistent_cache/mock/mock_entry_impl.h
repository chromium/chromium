// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_MOCK_MOCK_ENTRY_IMPL_H_
#define COMPONENTS_PERSISTENT_CACHE_MOCK_MOCK_ENTRY_IMPL_H_

#include <cstdint>

#include "components/persistent_cache/entry.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace persistent_cache {

class MockEntryImpl : public Entry {
 public:
  ~MockEntryImpl() override;

  // `Entry` overrides
  MOCK_METHOD(base::span<const uint8_t>, GetContentSpan, (), (const override));
  MOCK_METHOD(EntryMetadata, GetMetadata, (), (const override));

 protected:
  explicit MockEntryImpl();
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_MOCK_MOCK_ENTRY_IMPL_H_
