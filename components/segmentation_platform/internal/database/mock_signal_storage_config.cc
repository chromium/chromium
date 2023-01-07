// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/mock_signal_storage_config.h"

#include "components/segmentation_platform/internal/database/signal_storage_config.h"

namespace segmentation_platform {

MockSignalStorageConfig::MockSignalStorageConfig()
    : SignalStorageConfig(nullptr, nullptr) {}

MockSignalStorageConfig::~MockSignalStorageConfig() = default;

}  // namespace segmentation_platform
