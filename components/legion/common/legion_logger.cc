// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/common/legion_logger.h"

#include <string_view>

#include "base/logging.h"

namespace legion {

LegionLogger::LegionLogger() = default;

LegionLogger::~LegionLogger() = default;

void LegionLogger::LogInfo(std::string_view message) {
  VLOG(1) << message;
  for (auto& observer : observers_) {
    observer.OnLogInfo(message);
  }
}

void LegionLogger::LogError(std::string_view message) {
  LOG(ERROR) << message;
  for (auto& observer : observers_) {
    observer.OnLogError(message);
  }
}

void LegionLogger::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void LegionLogger::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace legion
