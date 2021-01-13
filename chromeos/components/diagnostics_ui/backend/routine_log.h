// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_ROUTINE_LOG_H_
#define CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_ROUTINE_LOG_H_

#include "base/files/file_path.h"
#include "chromeos/components/diagnostics_ui/mojom/system_routine_controller.mojom.h"

namespace chromeos {
namespace diagnostics {

// RoutineLog is used to record the status and outcome of Diagnostics Routines.
// Each time `LogRoutineStarted()` or `LogRoutineCompleted()` is called, a new
// line is appended to `routine_log_file_path`. The file is created before the
// first write if it does not exist.
class RoutineLog {
 public:
  explicit RoutineLog(const base::FilePath& routine_log_file_path);
  ~RoutineLog();

  RoutineLog(const RoutineLog&) = delete;
  RoutineLog& operator=(const RoutineLog&) = delete;

  // Adds an entry in the RoutineLog.
  void LogRoutineStarted(mojom::RoutineType type);
  void LogRoutineCompleted(mojom::RoutineType type,
                           mojom::StandardRoutineResult result);

 private:
  void AppendToLog(const std::string& content);

  const base::FilePath routine_log_file_path_;
};

}  // namespace diagnostics
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_ROUTINE_LOG_H_
