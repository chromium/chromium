// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/pending_backend.h"

#include <utility>

namespace persistent_cache {

PendingBackend::PendingBackend() = default;
PendingBackend::PendingBackend(sqlite_vfs::PendingFileSet pending_file_set)
    : pending_file_set(std::move(pending_file_set)) {}
PendingBackend::PendingBackend(PendingBackend&&) = default;
PendingBackend& PendingBackend::operator=(PendingBackend&&) = default;
PendingBackend::~PendingBackend() = default;

}  // namespace persistent_cache
