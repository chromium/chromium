// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYSTEM_CPU_PROCFS_STAT_CPU_PARSER_H_
#define COMPONENTS_SYSTEM_CPU_PROCFS_STAT_CPU_PARSER_H_

#include <stdint.h>

#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/system_cpu/core_times.h"

namespace system_cpu {

// Parses CPU time usage stats from procfs (/proc/stat).
//
// This class is not thread-safe. Each instance must be used on the same
// sequence, which must allow blocking I/O. The constructor may be used on a
// different sequence.
class ProcfsStatCpuParser {
 public:
  // The "production" `procfs_path` value for the constructor.
  static constexpr base::FilePath::CharType kProcfsStatPath[] =
      FILE_PATH_LITERAL("/proc/stat");

  // `stat_path` is exposed for testing. Production instances should be
  // constructed using base::FilePath(kProcfsStatPath).
  explicit ProcfsStatCpuParser(base::FilePath stat_path);
  ~ProcfsStatCpuParser();

  ProcfsStatCpuParser(const ProcfsStatCpuParser&) = delete;
  ProcfsStatCpuParser& operator=(const ProcfsStatCpuParser&) = delete;

  const std::vector<CoreTimes>& core_times() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return core_times_;
  }

  // Reads /proc/stat and updates the vector returned by core_times().
  //
  // Returns false if parsing fails. This only happens if /proc/stat is not
  // accessible. Invalid data is ignored.
  //
  // Cores without entries in /proc/stat will remain unchanged. Counters that
  // decrease below the last parsed value are ignored.
  bool Update();

 private:
  // Returns -1 if the line does not include any CPU.
  static int CoreIdFromLine(std::string_view stat_line);

  static void UpdateCore(std::string_view core_line, CoreTimes& core_times);

  SEQUENCE_CHECKER(sequence_checker_);

  const base::FilePath stat_path_;

  std::vector<CoreTimes> core_times_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace system_cpu

#endif  // COMPONENTS_SYSTEM_CPU_PROCFS_STAT_CPU_PARSER_H_
