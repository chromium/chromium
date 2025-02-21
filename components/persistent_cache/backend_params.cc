// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/backend_params.h"

namespace persistent_cache {

BackendParams::BackendParams() = default;
BackendParams::BackendParams(BackendParams&& other) = default;
BackendParams& BackendParams::operator=(BackendParams&& other) = default;
BackendParams::~BackendParams() = default;

BackendParams BackendParams::Copy() const {
  BackendParams params;
  params.db_file = db_file.Duplicate();
  params.db_file_is_writable = db_file_is_writable;
  params.journal_file = journal_file.Duplicate();
  params.journal_file_is_writable = journal_file_is_writable;
  params.type = type;
  return params;
}

}  // namespace persistent_cache
