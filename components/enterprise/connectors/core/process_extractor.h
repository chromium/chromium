// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_PROCESS_EXTRACTOR_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_PROCESS_EXTRACTOR_H_

#include <cstddef>
#include <string>
#include <string_view>
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

// Sanitizes a raw process or script path for telemetry reporting:
// - Replaces the current user's home directory prefix (from `base::DIR_HOME`)
//   with `<home_parent>/<USER>` (e.g. `/Users/<USER>`). Fails closed (returns
//   an empty string) if `base::DIR_HOME` cannot be resolved.
// - If the path exceeds `kMaxProcessPathLength` (1024 bytes), drops the
//   beginning of the string and keeps the trailing bytes starting on a valid
//   UTF-8 character boundary.
std::string SanitizeProcessPath(std::string_view raw_path);

// Sanitizes both `executable_path` and `script_path` on `node`.
void SanitizeProcessNodePaths(ProcessNode& node);

// Returns true if `executable_path` resolves to an allowlisted interpreter
// binary family (`node`, `nodejs`, `python`, `bash`, `sh`, `zsh`, `ruby`,
// `perl`, `deno`, `bun`, including version-suffixed binaries like `python3.12`
// or `node20`).
bool IsAllowlistedInterpreter(std::string_view executable_path);

// Extracts and sanitizes the script file argument from `argv` when
// `executable_path` is an allowlisted interpreter:
// - Omits `script_path` when known inline execution flags or subcommands
//   are present before the script target or `--` (e.g. `-e`, `-E`, `--eval`,
//   `-c`, `-lc`, `-ec`, `-s`, `-p`, `-m`, `-i`, bare `-`, or `eval`/`repl`).
// - Fails closed (returns an empty string) if no allowlisted script file
//   target can be identified or if `SanitizeProcessPath()` fails closed.
std::string ExtractInterpreterScript(std::string_view executable_path,
                                     const std::vector<std::string>& argv);

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_PROCESS_EXTRACTOR_H_
