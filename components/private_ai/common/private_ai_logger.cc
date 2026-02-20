// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/common/private_ai_logger.h"

#include <string_view>

#include "base/logging.h"

namespace private_ai {

PrivateAiLogger::PrivateAiLogger() = default;

PrivateAiLogger::~PrivateAiLogger() = default;

void PrivateAiLogger::LogInfo(const base::Location& location,
                              std::string_view message) {
  VLOG(1) << location.ToString() << ": " << message;
  for (auto& observer : observers_) {
    observer.OnLogInfo(location, message);
  }
}

void PrivateAiLogger::LogError(const base::Location& location,
                               std::string_view message) {
  LOG(ERROR) << location.ToString() << ": " << message;
  for (auto& observer : observers_) {
    observer.OnLogError(location, message);
  }
}

void PrivateAiLogger::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PrivateAiLogger::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace private_ai
