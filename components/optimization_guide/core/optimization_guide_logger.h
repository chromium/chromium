// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_LOGGER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_LOGGER_H_

#include <string>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/optimization_guide_common.mojom.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "url/gurl.h"

namespace optimization_guide {
class ModelExecutionInternalsPageBrowserTest;
}

#define OPTIMIZATION_GUIDE_LOGGER(log_source, optimization_guide_logger)     \
  OptimizationGuideLogger::LogMessageBuilder(log_source, __FILE__, __LINE__, \
                                             optimization_guide_logger)

// Interface to record the debug logs and send it to be shown in the
// optimization guide internals page.
class OptimizationGuideLogger {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnLogMessageAdded(
        base::Time event_time,
        optimization_guide_common::mojom::LogSource log_source,
        const std::string& source_file,
        int source_line,
        const std::string& message) = 0;
  };
  static OptimizationGuideLogger* GetInstance();
  OptimizationGuideLogger();
  ~OptimizationGuideLogger();

  OptimizationGuideLogger(const OptimizationGuideLogger&) = delete;
  OptimizationGuideLogger& operator=(const OptimizationGuideLogger&) = delete;

  void AddObserver(OptimizationGuideLogger::Observer* observer);
  void RemoveObserver(OptimizationGuideLogger::Observer* observer);
  void OnLogMessageAdded(base::Time event_time,
                         optimization_guide_common::mojom::LogSource log_source,
                         const std::string& source_file,
                         int source_line,
                         const std::string& message);

  // Whether debug logs should allowed to be recorded.
  bool ShouldEnableDebugLogs() const;

  base::WeakPtr<OptimizationGuideLogger> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Class that builds the log message and used when debugging is enabled via
  // command-line switch or the internals page.
  class LogMessageBuilder {
   public:
    LogMessageBuilder(optimization_guide_common::mojom::LogSource log_source,
                      const std::string& source_file,
                      int source_line,
                      OptimizationGuideLogger* optimization_guide_logger);
    ~LogMessageBuilder();

    LogMessageBuilder& operator<<(const char* message);
    LogMessageBuilder& operator<<(const std::string& message);
    LogMessageBuilder& operator<<(const GURL& url);
    LogMessageBuilder& operator<<(
        optimization_guide::proto::RequestContext request_context);
    LogMessageBuilder& operator<<(
        optimization_guide::proto::OptimizationType optimization_type);
    LogMessageBuilder& operator<<(optimization_guide::OptimizationTypeDecision
                                      optimization_type_decision);
    LogMessageBuilder& operator<<(optimization_guide::OptimizationGuideDecision
                                      optimization_guide_decision);
    LogMessageBuilder& operator<<(
        optimization_guide::proto::OptimizationTarget optimization_target);

   private:
    const optimization_guide_common::mojom::LogSource log_source_;
    const std::string source_file_;
    const int source_line_;
    std::vector<std::string> messages_;
    raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;
  };

 private:
  friend class optimization_guide::ModelExecutionInternalsPageBrowserTest;
  friend class NewTabPageUtilBrowserTest;

  struct LogMessage {
    LogMessage(base::Time event_time,
               optimization_guide_common::mojom::LogSource log_source,
               const std::string& source_file,
               int source_line,
               const std::string& message);
    const base::Time event_time;
    const optimization_guide_common::mojom::LogSource log_source;
    const std::string source_file;
    const int source_line;
    const std::string message;
  };

  // Contains the most recent log messages. Messages are queued up only when
  // |kDebugLoggingEnabled| command-line switch is specified. This allows the
  // messages at startup to be saved and shown in the internals page later.
  base::circular_deque<LogMessage> recent_log_messages_;

  base::ObserverList<OptimizationGuideLogger::Observer> observers_;

  bool command_line_flag_enabled_ = false;

  base::WeakPtrFactory<OptimizationGuideLogger> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_LOGGER_H_
