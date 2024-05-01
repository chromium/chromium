// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/file_manager/indexing/file_index_service.h"

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/logging.h"

namespace ash::file_manager {

FileIndexService::FileIndexService(base::FilePath profile_path)
    : profile_path_(std::move(profile_path)) {}

FileIndexService::~FileIndexService() = default;

}  // namespace ash::file_manager
