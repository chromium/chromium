// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/pending_backend.h"

namespace persistent_cache {

PendingBackend::PendingBackend() = default;
PendingBackend::PendingBackend(PendingBackend&&) = default;
PendingBackend& PendingBackend::operator=(PendingBackend&&) = default;
PendingBackend::~PendingBackend() = default;

PendingBackend::SqliteData::SqliteData() = default;
PendingBackend::SqliteData::~SqliteData() = default;
PendingBackend::SqliteData::SqliteData(SqliteData&&) = default;
PendingBackend::SqliteData& PendingBackend::SqliteData::operator=(
    SqliteData&&) = default;

}  // namespace persistent_cache
