// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/common/legion_logger.h"

#include <string_view>

#include "base/logging.h"

namespace private_ai {

LegionLogger::LegionLogger() = default;

LegionLogger::~LegionLogger() = default;

void LegionLogger::LogInfo(const base::Location& location,
                           std::string_view message) {
  VLOG(1) << location.ToString() << ": " << message;
  for (auto& observer : observers_) {
    observer.OnLogInfo(location, message);
  }
}

void LegionLogger::LogError(const base::Location& location,
                            std::string_view message) {
  LOG(ERROR) << location.ToString() << ": " << message;
  for (auto& observer : observers_) {
    observer.OnLogError(location, message);
  }
}

void LegionLogger::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void LegionLogger::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace private_ai
