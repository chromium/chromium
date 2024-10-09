// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/filesystem/shared_temp_dir.h"

#include "base/files/scoped_temp_dir.h"

namespace filesystem {

SharedTempDir::SharedTempDir(std::unique_ptr<base::ScopedTempDir> temp_dir)
    : temp_dir_(std::move(temp_dir)) {}

SharedTempDir::~SharedTempDir() = default;

}  // namespace filesystem
