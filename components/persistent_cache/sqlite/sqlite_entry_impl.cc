// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/sqlite/sqlite_entry_impl.h"

#include <cstdint>

#include "base/containers/span.h"

namespace persistent_cache {

SqliteEntryImpl::SqliteEntryImpl(std::string&& content)
    : content_(std::move(content)) {}

SqliteEntryImpl::~SqliteEntryImpl() = default;

base::span<const uint8_t> SqliteEntryImpl::GetContentSpan() const {
  return base::as_byte_span(content_);
}

}  // namespace persistent_cache
