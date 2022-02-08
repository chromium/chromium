// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_LOGGER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_LOGGER_H_

#include <string>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"

// Interface to record the debug logs and send it to be shown in the
// optimization guide internals page.
class OptimizationGuideLogger {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnLogMessageAdded(base::Time event_time,
                                   const std::string& source_file,
                                   int source_line,
                                   const std::string& message) = 0;
  };
  OptimizationGuideLogger();
  ~OptimizationGuideLogger();

  OptimizationGuideLogger(const OptimizationGuideLogger&) = delete;
  OptimizationGuideLogger& operator=(const OptimizationGuideLogger&) = delete;

  void AddObserver(OptimizationGuideLogger::Observer* observer);
  void RemoveObserver(OptimizationGuideLogger::Observer* observer);
  void OnLogMessageAdded(base::Time event_time,
                         const std::string& source_file,
                         int source_line,
                         const std::string& message);

  // Whether debug logs should allowed to be recorded.
  bool ShouldEnableDebugLogs() const;

 private:
  base::ObserverList<OptimizationGuideLogger::Observer> observers_;
};

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_LOGGER_H_
