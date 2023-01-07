// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/test/mock_file_monitor.h"

namespace download {

MockFileMonitor::MockFileMonitor() = default;
MockFileMonitor::~MockFileMonitor() = default;

void MockFileMonitor::TriggerInit(bool success) {
  std::move(init_callback_).Run(success);
}

void MockFileMonitor::TriggerHardRecover(bool success) {
  std::move(recover_callback_).Run(success);
}

void MockFileMonitor::Initialize(FileMonitor::InitCallback callback) {
  init_callback_ = std::move(callback);
}

void MockFileMonitor::HardRecover(FileMonitor::InitCallback callback) {
  recover_callback_ = std::move(callback);
}

}  // namespace download