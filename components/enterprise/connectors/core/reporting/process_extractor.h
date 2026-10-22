// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_PROCESS_EXTRACTOR_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_PROCESS_EXTRACTOR_H_

#include <cstddef>
#include <string>
#include <vector>

#include "base/process/process_handle.h"

namespace enterprise_connectors {

// Maximum number of process nodes walked from the immediate connecting process
// up to the root ancestor.
inline constexpr size_t kMaxProcessAncestryDepth = 16;

// Maximum length (in bytes) of sanitized executable_path and script_path
// strings.
inline constexpr size_t kMaxProcessPathLength = 1024;

// A single resolved node in an external process's OS ancestry.
struct ProcessNode {
  ProcessNode();
  ProcessNode(base::ProcessId process_id,
              base::ProcessId parent_process_id,
              std::string executable_path,
              std::string script_path);
  ProcessNode(const ProcessNode&);
  ProcessNode& operator=(const ProcessNode&);
  ProcessNode(ProcessNode&&) noexcept;
  ProcessNode& operator=(ProcessNode&&) noexcept;
  ~ProcessNode();

  friend bool operator==(const ProcessNode&, const ProcessNode&) = default;

  base::ProcessId process_id = base::kNullProcessId;
  base::ProcessId parent_process_id = base::kNullProcessId;
  std::string executable_path;
  std::string script_path;
};

// Extracted process ancestry for an external process, ordered from the
// immediate process (index 0) up to the root ancestor (bounded to
// kMaxProcessAncestryDepth).
struct ProcessTree {
  bool empty() const { return process_nodes.empty(); }

  friend bool operator==(const ProcessTree&, const ProcessTree&) = default;

  std::vector<ProcessNode> process_nodes;
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_PROCESS_EXTRACTOR_H_
