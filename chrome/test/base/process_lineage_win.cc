// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/process_lineage_win.h"

#include <sstream>

#include "base/process/process.h"
#include "base/strings/string_util.h"
#include "chrome/test/base/process_inspector_win.h"

// static
ProcessLineage ProcessLineage::Create(DWORD process_id) {
  std::vector<ProcessProperties> properties;

  while (true) {
    base::Process process = base::Process::OpenWithAccess(
        process_id, PROCESS_QUERY_INFORMATION | SYNCHRONIZE | PROCESS_VM_READ);
    if (!process.IsValid())
      break;

    auto inspector = ProcessInspector::Create(process);
    if (!inspector)
      break;

    // If PID reuse proves to be a problem, this would be a good point to add
    // extra checks that |process| is older than the previously inspected
    // process.

    properties.push_back({process_id, inspector->command_line()});
    DWORD parent_pid = inspector->GetParentPid();
    if (process_id == parent_pid)
      break;
    process_id = parent_pid;
  }
  return ProcessLineage(std::move(properties));
}

ProcessLineage::ProcessLineage(ProcessLineage&& other) = default;
ProcessLineage& ProcessLineage::operator=(ProcessLineage&& other) = default;
ProcessLineage::~ProcessLineage() = default;

std::wstring ProcessLineage::ToString() const {
  std::wostringstream sstream;
  std::wstring sep;
  for (const auto& prop : lineage_) {
    sstream << sep << L"(process_id: " << prop.process_id
            << L", command_line: \"" << prop.command_line << L"\")";
    if (sep.empty())
      sep = L", ";
  }
  return sstream.str();
}

ProcessLineage::ProcessLineage(std::vector<ProcessProperties> lineage)
    : lineage_(std::move(lineage)) {}
