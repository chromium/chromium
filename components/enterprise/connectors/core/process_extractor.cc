// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/process_extractor.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"

namespace enterprise_connectors {

namespace {

// TODO(crbug.com/575478024): Scrub multiple occurrences of the home directory
// in a follow-up CL.
std::string ScrubHomePrefix(std::string_view raw_path) {
  base::FilePath home_dir;
  if (!base::PathService::Get(base::DIR_HOME, &home_dir) || home_dir.empty()) {
    return std::string();
  }
  home_dir = home_dir.StripTrailingSeparators();
  if (home_dir == home_dir.DirName()) {
    return std::string();
  }
  const base::FilePath file_path =
      base::FilePath::FromUTF8Unsafe(raw_path).StripTrailingSeparators();
  const base::FilePath masked_home =
      home_dir.DirName().AppendASCII("<USER>");
  if (file_path == home_dir) {
    return masked_home.AsUTF8Unsafe();
  }
  base::FilePath masked_path = masked_home;
  if (home_dir.AppendRelativePath(file_path, &masked_path)) {
    return masked_path.AsUTF8Unsafe();
  }
  return std::string(raw_path);
}

std::string TruncatePathFromFrontUtf8(std::string path, size_t max_bytes) {
  if (path.size() <= max_bytes) {
    return path;
  }
  size_t start = path.size() - max_bytes;
  // Advance past any UTF-8 continuation bytes (10xxxxxx) so the retained
  // suffix starts on a valid UTF-8 codepoint boundary.
  while (start < path.size() &&
         (static_cast<uint8_t>(path[start]) & 0xC0) == 0x80) {
    ++start;
  }
  return path.substr(start);
}

}  // namespace

ProcessNode::ProcessNode() = default;

ProcessNode::ProcessNode(base::ProcessId process_id,
                         base::ProcessId parent_process_id,
                         std::string executable_path,
                         std::string script_path)
    : process_id(process_id),
      parent_process_id(parent_process_id),
      executable_path(std::move(executable_path)),
      script_path(std::move(script_path)) {}

ProcessNode::ProcessNode(const ProcessNode&) = default;
ProcessNode& ProcessNode::operator=(const ProcessNode&) = default;
ProcessNode::ProcessNode(ProcessNode&&) noexcept = default;
ProcessNode& ProcessNode::operator=(ProcessNode&&) noexcept = default;
ProcessNode::~ProcessNode() = default;

std::string SanitizeProcessPath(std::string_view raw_path) {
  if (raw_path.empty()) {
    return std::string();
  }
  return TruncatePathFromFrontUtf8(ScrubHomePrefix(raw_path),
                                   kMaxProcessPathLength);
}

void SanitizeProcessNodePaths(ProcessNode& node) {
  if (!node.executable_path.empty()) {
    node.executable_path = SanitizeProcessPath(node.executable_path);
  }
  if (!node.script_path.empty()) {
    node.script_path = SanitizeProcessPath(node.script_path);
  }
}

}  // namespace enterprise_connectors
