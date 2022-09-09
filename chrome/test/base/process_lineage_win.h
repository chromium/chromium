// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_PROCESS_LINEAGE_WIN_H_
#define CHROME_TEST_BASE_PROCESS_LINEAGE_WIN_H_

#include <string>
#include <utility>
#include <vector>

#include "base/win/windows_types.h"

// Holds the lineage of a process; specifically, the pid and command line of the
// process and each of its ancestors that are still running. Due to aggressive
// pid reuse on Windows, it's possible that the collection may contain
// misleading information. Since the goal of this method is to get useful data
// in the aggregate, some misleading info here and there is tolerable. If it
// proves to be intolerable, additional checks can be added to be sure that each
// ancestor is older that its child.
class ProcessLineage {
 public:
  struct ProcessProperties {
    DWORD process_id;
    std::wstring command_line;
  };

  // Returns the lineage of |process_id| or an empty instance in case of error.
  static ProcessLineage Create(DWORD process_id);

  ProcessLineage(ProcessLineage&& other);
  ProcessLineage& operator=(ProcessLineage&& other);
  ~ProcessLineage();

  bool IsEmpty() const { return lineage_.empty(); }
  std::wstring ToString() const;

  ProcessLineage(const ProcessLineage&) = delete;
  ProcessLineage& operator=(const ProcessLineage&) = delete;

 private:
  explicit ProcessLineage(std::vector<ProcessProperties> lineage);

  std::vector<ProcessProperties> lineage_;
};

#endif  // CHROME_TEST_BASE_PROCESS_LINEAGE_WIN_H_
