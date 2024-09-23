// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_logger.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/strings/strcat.h"
#include "components/optimization_guide/core/hints_processing_util.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"

namespace {

// TODO(rajendrant): Verify if all debug messages before browser startup are
// getting saved without being dropped, when some hints fetching and model
// downloading happens.
constexpr size_t kMaxRecentLogMessages = 100;

}  // namespace

OptimizationGuideLogger::LogMessageBuilder::LogMessageBuilder(
    optimization_guide_common::mojom::LogSource log_source,
    const std::string& source_file,
    int source_line,
    OptimizationGuideLogger* optimization_guide_logger)
    : log_source_(log_source),
      source_file_(source_file),
      source_line_(source_line),
      optimization_guide_logger_(optimization_guide_logger) {}

OptimizationGuideLogger::LogMessageBuilder::~LogMessageBuilder() {
  if (!optimization_guide_logger_) {
    // It is possible for this to not be available in tests, so just return
    // here.
    return;
  }

  std::string message = base::StrCat(messages_);
  optimization_guide_logger_->OnLogMessageAdded(
      base::Time::Now(), log_source_, source_file_, source_line_, message);
  DVLOG(1) << source_file_ << "(" << source_line_ << ") " << message;
}

OptimizationGuideLogger::LogMessageBuilder&
OptimizationGuideLogger::LogMessageBuilder::operator<<(const char* message) {
  messages_.push_back(message);
  return *this;
}

OptimizationGuideLogger::LogMessageBuilder&
OptimizationGuideLogger::LogMessageBuilder::operator<<(
    const std::string& message) {
  messages_.push_back(message);
  return *this;
}

OptimizationGuideLogger::LogMessageBuilder&
OptimizationGuideLogger::LogMessageBuilder::operator<<(const GURL& url) {
  messages_.push_back(url.possibly_invalid_spec());
  return *this;
}

OptimizationGuideLogger::LogMessageBuilder&
OptimizationGuideLogger::LogMessageBuilder::operator<<(
    optimization_guide::proto::RequestContext request_context) {
  messages_.push_back(
      optimization_guide::proto::RequestContext_Name(request_context));
  return *this;
}

OptimizationGuideLogger::LogMessageBuilder&
OptimizationGuideLogger::LogMessageBuilder::operator<<(
    optimization_guide::proto::OptimizationType optimization_type) {
  messages_.push_back(
      optimization_guide::GetStringNameForOptimizationType(optimization_type));
  return *this;
}

OptimizationGuideLogger::LogMessageBuilder&
OptimizationGuideLogger::LogMessageBuilder::operator<<(
    optimization_guide::OptimizationTypeDecision optimization_type_decision) {
  messages_.push_back(
      base::NumberToString(static_cast<int>(optimization_type_decision)));
  return *this;
}

OptimizationGuideLogger::LogMessageBuilder&
OptimizationGuideLogger::LogMessageBuilder::operator<<(
    optimization_guide::OptimizationGuideDecision optimization_guide_decision) {
  messages_.push_back(
      GetStringForOptimizationGuideDecision(optimization_guide_decision));
  return *this;
}

OptimizationGuideLogger::LogMessageBuilder&
OptimizationGuideLogger::LogMessageBuilder::operator<<(
    optimization_guide::proto::OptimizationTarget optimization_target) {
  messages_.push_back(
      optimization_guide::proto::OptimizationTarget_Name(optimization_target));
  return *this;
}

OptimizationGuideLogger::LogMessage::LogMessage(
    base::Time event_time,
    optimization_guide_common::mojom::LogSource log_source,
    const std::string& source_file,
    int source_line,
    const std::string& message)
    : event_time(event_time),
      log_source(log_source),
      source_file(source_file),
      source_line(source_line),
      message(message) {}

// static
OptimizationGuideLogger* OptimizationGuideLogger::GetInstance() {
  static base::NoDestructor<OptimizationGuideLogger> instance;
  return instance.get();
}

OptimizationGuideLogger::OptimizationGuideLogger()
    : command_line_flag_enabled_(
          optimization_guide::switches::IsDebugLogsEnabled()) {
  if (command_line_flag_enabled_) {
    recent_log_messages_.reserve(kMaxRecentLogMessages);
  }
}

OptimizationGuideLogger::~OptimizationGuideLogger() = default;

void OptimizationGuideLogger::AddObserver(
    OptimizationGuideLogger::Observer* observer) {
  observers_.AddObserver(observer);
  if (command_line_flag_enabled_) {
    for (const auto& message : recent_log_messages_) {
      for (Observer& obs : observers_) {
        obs.OnLogMessageAdded(message.event_time, message.log_source,
                              message.source_file, message.source_line,
                              message.message);
      }
    }
  }
}

void OptimizationGuideLogger::RemoveObserver(
    OptimizationGuideLogger::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void OptimizationGuideLogger::OnLogMessageAdded(
    base::Time event_time,
    optimization_guide_common::mojom::LogSource log_source,
    const std::string& source_file,
    int source_line,
    const std::string& message) {
  if (command_line_flag_enabled_) {
    recent_log_messages_.emplace_back(event_time, log_source, source_file,
                                      source_line, message);
    if (recent_log_messages_.size() > kMaxRecentLogMessages)
      recent_log_messages_.pop_front();
  }
  for (Observer& obs : observers_)
    obs.OnLogMessageAdded(event_time, log_source, source_file, source_line,
                          message);
}

bool OptimizationGuideLogger::ShouldEnableDebugLogs() const {
  return !observers_.empty() || command_line_flag_enabled_;
}
