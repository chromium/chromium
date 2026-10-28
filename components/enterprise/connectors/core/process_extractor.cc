// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/process_extractor.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/containers/fixed_flat_set.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"

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
  const base::FilePath masked_home = home_dir.DirName().AppendASCII("<USER>");
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

std::string NormalizeInterpreterName(std::string_view executable_path) {
  base::FilePath base_path = base::FilePath::FromUTF8Unsafe(executable_path)
                                 .StripTrailingSeparators()
                                 .BaseName();
  if (base::EqualsCaseInsensitiveASCII(
          base::FilePath(base_path.FinalExtension()).AsUTF8Unsafe(), ".exe")) {
    base_path = base_path.RemoveFinalExtension();
  }
  const std::string lower_base = base::ToLowerASCII(base_path.AsUTF8Unsafe());
  return std::string(
      base::TrimString(lower_base, "0123456789._-", base::TRIM_TRAILING));
}

bool IsAllowlistedInterpreterName(std::string_view interp_name) {
  static constexpr auto kAllowlistedInterpreters =
      base::MakeFixedFlatSet<std::string_view>({"bash", "bun", "deno", "node",
                                                "nodejs", "perl", "python",
                                                "ruby", "sh", "zsh"});
  return kAllowlistedInterpreters.contains(interp_name);
}

bool IsInlineExecutionFlag(std::string_view arg, std::string_view interp_name) {
  static constexpr auto kShellInterpreters =
      base::MakeFixedFlatSet<std::string_view>({"bash", "sh", "zsh"});
  if (kShellInterpreters.contains(interp_name)) {
    return arg == "-" ||
           (base::StartsWith(arg, "-", base::CompareCase::SENSITIVE) &&
            !base::StartsWith(arg, "--", base::CompareCase::SENSITIVE) &&
            arg.find_first_of("cs") != std::string_view::npos);
  }
  static constexpr auto kInlineFlagsAndSubcommands =
      base::MakeFixedFlatSet<std::string_view>({"-", "--eval", "--interactive",
                                                "--module", "--print", "-i",
                                                "-m", "-p", "eval", "repl"});
  static constexpr std::string_view kInlineFlagPrefixes[] = {
      "-c", "-e", "-E", "--eval=", "-p=", "--print="};
  if (kInlineFlagsAndSubcommands.contains(arg)) {
    return true;
  }
  for (std::string_view prefix : kInlineFlagPrefixes) {
    if (base::StartsWith(arg, prefix, base::CompareCase::SENSITIVE)) {
      return true;
    }
  }
  return false;
}

bool LooksLikeScriptTarget(std::string_view arg) {
  if (arg.empty() || base::StartsWith(arg, "-", base::CompareCase::SENSITIVE) ||
      base::StartsWith(arg, "~", base::CompareCase::SENSITIVE) ||
      arg.find_first_of(" \t\r\n();{}?#=") != std::string_view::npos ||
      arg.find("://") != std::string_view::npos) {
    return false;
  }
  const base::FilePath file_path = base::FilePath::FromUTF8Unsafe(arg);
  if (file_path.ReferencesParent()) {
    return false;
  }
  const std::string final_ext = base::ToLowerASCII(
      base::FilePath(file_path.FinalExtension()).AsUTF8Unsafe());
  static constexpr auto kScriptExtensions =
      base::MakeFixedFlatSet<std::string_view>(
          {".bash", ".cjs", ".cts", ".js", ".jsx", ".mjs", ".mts", ".pl", ".pm",
           ".py", ".pyc", ".pyw", ".rb", ".sh", ".ts", ".tsx", ".zsh"});
  return kScriptExtensions.contains(final_ext);
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

bool IsAllowlistedInterpreter(std::string_view executable_path) {
  return IsAllowlistedInterpreterName(
      NormalizeInterpreterName(executable_path));
}

std::string ExtractInterpreterScript(std::string_view executable_path,
                                     const std::vector<std::string>& argv) {
  if (argv.size() <= 1) {
    return std::string();
  }
  const std::string interp_name = NormalizeInterpreterName(executable_path);
  if (!IsAllowlistedInterpreterName(interp_name)) {
    return std::string();
  }

  for (size_t i = 1; i < argv.size(); ++i) {
    const std::string_view arg = argv[i];
    if (arg == "--") {
      if (i + 1 < argv.size() && LooksLikeScriptTarget(argv[i + 1])) {
        return SanitizeProcessPath(argv[i + 1]);
      }
      return std::string();
    }
    if (IsInlineExecutionFlag(arg, interp_name)) {
      return std::string();
    }
    if (LooksLikeScriptTarget(arg)) {
      return SanitizeProcessPath(arg);
    }
  }
  return std::string();
}

}  // namespace enterprise_connectors
