// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/sqlite/vfs/sandboxed_file.h"

namespace persistent_cache {

SandboxedFile::SandboxedFile(base::File file, AccessRights access_rights)
    : underlying_file_(std::move(file)), access_rights_(access_rights) {}
SandboxedFile::~SandboxedFile() = default;

SandboxedFile::SandboxedFile(SandboxedFile&& other) = default;
SandboxedFile& SandboxedFile::operator=(SandboxedFile&& other) = default;

SandboxedFile SandboxedFile::Copy() const {
  return SandboxedFile(DuplicateUnderlyingFile(), access_rights_);
}

base::File SandboxedFile::DuplicateUnderlyingFile() const {
  return underlying_file_.Duplicate();
}

}  // namespace persistent_cache
