// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/mock_ukm_database.h"

#include "base/files/file_path.h"

namespace segmentation_platform {

MockUkmDatabase::MockUkmDatabase() : UkmDatabase(base::FilePath()) {}
MockUkmDatabase::~MockUkmDatabase() = default;

}  // namespace segmentation_platform
