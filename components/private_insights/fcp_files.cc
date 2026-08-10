// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_insights/fcp_files.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"

namespace private_insights {

FcpFiles::FcpFiles() = default;
FcpFiles::~FcpFiles() = default;

absl::StatusOr<std::string> FcpFiles::CreateTempFile(  // nocheck
    const std::string& /*prefix*/,
    const std::string& /*suffix*/) {
  base::FilePath temp_file;
  if (!base::CreateTemporaryFile(&temp_file)) {
    return absl::InvalidArgumentError("Failed to create temporary file");
  }
  return temp_file.AsUTF8Unsafe();
}

}  // namespace private_insights
