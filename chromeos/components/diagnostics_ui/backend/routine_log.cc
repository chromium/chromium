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

const char kNewline[] = "\n";
const char kSeparator[] = " - ";
const char kStartedDescription[] = "Started";

std::string GetCurrentTimeAsString() {
  return base::UTF16ToUTF8(
      base::TimeFormatTimeOfDayWithMilliseconds(base::Time::Now()));
}

}  // namespace

RoutineLog::RoutineLog(const base::FilePath& routine_log_file_path)
    : routine_log_file_path_(routine_log_file_path) {}

RoutineLog::~RoutineLog() = default;

void RoutineLog::LogRoutineStarted(mojom::RoutineType type) {
  if (!base::PathExists(routine_log_file_path_)) {
    base::WriteFile(routine_log_file_path_, "");
  }

  std::stringstream log_line;
  log_line << GetCurrentTimeAsString() << kSeparator << type << kSeparator
           << kStartedDescription << kNewline;
  AppendToLog(log_line.str());
}

void RoutineLog::LogRoutineCompleted(mojom::RoutineType type,
                                     mojom::StandardRoutineResult result) {
  DCHECK(base::PathExists(routine_log_file_path_));

  std::stringstream log_line;
  log_line << GetCurrentTimeAsString() << kSeparator << type << kSeparator
           << result << kNewline;
  AppendToLog(log_line.str());
}

void RoutineLog::AppendToLog(const std::string& content) {
  base::AppendToFile(routine_log_file_path_, content.data(), content.size());
}

}  // namespace diagnostics
}  // namespace chromeos
