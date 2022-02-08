// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_logger.h"

OptimizationGuideLogger::OptimizationGuideLogger() = default;

OptimizationGuideLogger::~OptimizationGuideLogger() = default;

void OptimizationGuideLogger::AddObserver(
    OptimizationGuideLogger::Observer* observer) {
  observers_.AddObserver(observer);
}

void OptimizationGuideLogger::RemoveObserver(
    OptimizationGuideLogger::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void OptimizationGuideLogger::OnLogMessageAdded(base::Time event_time,
                                                const std::string& source_file,
                                                int source_line,
                                                const std::string& message) {
  DCHECK(!observers_.empty());
  for (Observer& obs : observers_)
    obs.OnLogMessageAdded(event_time, source_file, source_line, message);
}

bool OptimizationGuideLogger::ShouldEnableDebugLogs() const {
  return !observers_.empty();
}
