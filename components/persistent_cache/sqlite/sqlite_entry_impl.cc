// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/sqlite/sqlite_entry_impl.h"

#include <stdint.h>

#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/types/pass_key.h"
#include "components/persistent_cache/sqlite/sqlite_backend_impl.h"
#include "sql/statement.h"

namespace persistent_cache {

SqliteEntryImpl::SqliteEntryImpl(SqliteBackendImpl& backend,
                                 std::unique_ptr<sql::Statement> statement,
                                 base::span<const uint8_t> content,
                                 EntryMetadata metadata)
    : backend_(backend),
      statement_(std::move(statement)),
      content_(content),
      metadata_(metadata) {}

SqliteEntryImpl::~SqliteEntryImpl() {
  content_ = base::raw_span<const uint8_t>();
  backend_->FinalizeStatement(std::move(statement_));
}

// static
std::unique_ptr<SqliteEntryImpl> SqliteEntryImpl::MakeUnique(
    base::PassKey<SqliteBackendImpl> passkey,
    SqliteBackendImpl& backend,
    std::unique_ptr<sql::Statement> statement,
    base::span<const uint8_t> content,
    EntryMetadata metadata) {
  // Avoid `make_unique` as it requires friending it which in turn lets any
  // class create `unique_ptr`s of this class.
  return base::WrapUnique<SqliteEntryImpl>(
      new SqliteEntryImpl(backend, std::move(statement), content, metadata));
}

base::span<const uint8_t> SqliteEntryImpl::GetContentSpan() const {
  return content_;
}

EntryMetadata SqliteEntryImpl::GetMetadata() const {
  return metadata_;
}

}  // namespace persistent_cache
