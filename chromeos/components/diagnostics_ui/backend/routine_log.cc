// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/routine_log.h"

#include <sstream>
#include <string>

#include "base/files/file_util.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"

namespace chromeos {
namespace diagnostics {
namespace {

const char kCancelledDescription[] = "Inflight Routine Cancelled";
const char kNewline[] = "\n";
const char kSeparator[] = " - ";
const char kStartedDescription[] = "Started";

std::string GetCurrentDateTimeAsString() {
  return base::UTF16ToUTF8(base::TimeFormatShortDateAndTime(base::Time::Now()));
}

std::string getRoutineResultString(mojom::StandardRoutineResult result) {
  switch (result) {
    case mojom::StandardRoutineResult::kTestPassed:
      return "Passed";
    case mojom::StandardRoutineResult::kTestFailed:
      return "Failed";
    case mojom::StandardRoutineResult::kExecutionError:
      return "Execution error";
    case mojom::StandardRoutineResult::kUnableToRun:
      return "Unable to run";
  }
}

std::string getRoutineTypeString(mojom::RoutineType type) {
  std::stringstream s;
  s << type;
  const std::string routineName = s.str();

  // Remove leading "k" ex: "kCpuStress" -> "CpuStress".
  DCHECK_GE(routineName.size(), 1U);
  DCHECK_EQ(routineName[0], 'k');
  return routineName.substr(1, routineName.size() - 1);
}

}  // namespace

RoutineLog::RoutineLog(const base::FilePath& routine_log_file_path)
    : routine_log_file_path_(routine_log_file_path) {}

RoutineLog::~RoutineLog() = default;

void RoutineLog::LogRoutineStarted(mojom::RoutineType type) {
  if (!base::PathExists(routine_log_file_path_)) {
    CreateFile();
  }

  std::stringstream log_line;
  log_line << GetCurrentDateTimeAsString() << kSeparator
           << getRoutineTypeString(type) << kSeparator << kStartedDescription
           << kNewline;
  AppendToLog(log_line.str());
}

void RoutineLog::LogRoutineCompleted(mojom::RoutineType type,
                                     mojom::StandardRoutineResult result) {
  DCHECK(base::PathExists(routine_log_file_path_));

  std::stringstream log_line;
  log_line << GetCurrentDateTimeAsString() << kSeparator
           << getRoutineTypeString(type) << kSeparator
           << getRoutineResultString(result) << kNewline;
  AppendToLog(log_line.str());
}

void RoutineLog::LogRoutineCancelled() {
  DCHECK(base::PathExists(routine_log_file_path_));

  std::stringstream log_line;
  log_line << GetCurrentDateTimeAsString() << kSeparator
           << kCancelledDescription << kNewline;
  AppendToLog(log_line.str());
}

std::string RoutineLog::GetContents() const {
  if (!base::PathExists(routine_log_file_path_)) {
    return "";
  }

  std::string contents;
  base::ReadFileToString(routine_log_file_path_, &contents);

  return contents;
}

void RoutineLog::AppendToLog(const std::string& content) {
  base::AppendToFile(routine_log_file_path_, content.data(), content.size());
}

void RoutineLog::CreateFile() {
  DCHECK(!base::PathExists(routine_log_file_path_));

  if (!base::PathExists(routine_log_file_path_.DirName())) {
    const bool create_dir_success =
        base::CreateDirectory(routine_log_file_path_.DirName());
    if (!create_dir_success) {
      LOG(ERROR) << "Failed to create Diagnostics Routine Log directory.";
      return;
    }
  }

  const bool create_file_success = base::WriteFile(routine_log_file_path_, "");
  if (!create_file_success) {
    LOG(ERROR) << "Failed to create Diagnostics Routine Log file.";
  }
}

}  // namespace diagnostics
}  // namespace chromeos
