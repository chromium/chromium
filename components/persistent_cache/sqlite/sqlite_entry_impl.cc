// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/sqlite/sqlite_entry_impl.h"

#include <cstdint>

#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/types/pass_key.h"

namespace persistent_cache {

SqliteEntryImpl::SqliteEntryImpl(std::string&& content, EntryMetadata metadata)
    : content_(std::move(content)), metadata_(metadata) {}
SqliteEntryImpl::~SqliteEntryImpl() = default;

// static
std::unique_ptr<SqliteEntryImpl> SqliteEntryImpl::MakeUnique(
    base::PassKey<SqliteBackendImpl> passkey,
    std::string&& content,
    EntryMetadata metadata) {
  // Avoid `make_unique` as it requires friending it which in turn lets any
  // class create `unique_ptr`s of this class.
  auto ptr = base::WrapUnique<SqliteEntryImpl>(
      new SqliteEntryImpl(std::move(content), metadata));
  return ptr;
}

base::span<const uint8_t> SqliteEntryImpl::GetContentSpan() const {
  return base::as_byte_span(content_);
}

EntryMetadata SqliteEntryImpl::GetMetadata() const {
  return metadata_;
}

}  // namespace persistent_cache
