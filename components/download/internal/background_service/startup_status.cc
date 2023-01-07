// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/startup_status.h"

#include "base/check.h"

namespace download {

StartupStatus::StartupStatus() = default;
StartupStatus::~StartupStatus() = default;

void StartupStatus::Reset() {
  driver_ok.reset();
  model_ok.reset();
  file_monitor_ok.reset();
}

bool StartupStatus::Complete() const {
  return driver_ok.has_value() && model_ok.has_value() &&
         file_monitor_ok.has_value();
}

bool StartupStatus::Ok() const {
  DCHECK(Complete());
  return driver_ok.value() && model_ok.value() && file_monitor_ok.value();
}

bool StartupStatus::Failed() const {
  return (driver_ok && !driver_ok.value()) || (model_ok && !model_ok.value()) ||
         (file_monitor_ok && !file_monitor_ok.value());
}

}  // namespace download
