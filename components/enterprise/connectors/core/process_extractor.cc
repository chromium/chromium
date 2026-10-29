// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/process_extractor.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/strings/string_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"

// TODO(crbug.com/575478024): Remove redundant `IS_MAC` / `IS_POSIX` platform
// guards while `process_extractor.cc` is gated on `enterprise_cdp_attribution`
// (`is_mac` in `features.gni`), or split into platform-specific files when
// expanding beyond macOS.
#if BUILDFLAG(IS_MAC)
#include <libproc.h>
#include <sys/proc_info.h>
#include <sys/sysctl.h>
#endif

namespace enterprise_connectors {

namespace {

bool IsValidUserProcessId(base::ProcessId pid) {
#if BUILDFLAG(IS_POSIX)
  // On POSIX, PID 0 is the kernel scheduler and PID 1 is init/launchd.
  return pid > 1;
#else
  return pid != base::kNullProcessId;
#endif
}

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

std::optional<internal::RawProcessInfo> QueryOsProcessInfo(
    base::ProcessId pid) {
  if (!IsValidUserProcessId(pid)) {
    return std::nullopt;
  }

  internal::RawProcessInfo info;
#if BUILDFLAG(IS_POSIX)
  // On POSIX, `base::ProcessHandle` is `base::ProcessId` (`pid_t`).
  info.parent_process_id = base::GetParentProcessId(pid);
  base::FilePath exec_path = base::GetProcessExecutablePath(pid);
  if (!exec_path.empty()) {
    info.executable_path = exec_path.AsUTF8Unsafe();
  }
#endif

#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/575478024): Add Windows and Linux argv/process extraction
  // support when enabling ENTERPRISE_CDP_ATTRIBUTION on other platforms.
  if (info.parent_process_id <= 0) {
    proc_bsdinfo bsd_info = {};
    if (proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &bsd_info, sizeof(bsd_info)) ==
        sizeof(bsd_info)) {
      info.parent_process_id = static_cast<base::ProcessId>(bsd_info.pbi_ppid);
    }
  }
  if (info.executable_path.empty()) {
    char pathbuf[PROC_PIDPATHINFO_MAXSIZE] = {};
    if (proc_pidpath(pid, pathbuf, sizeof(pathbuf)) > 0) {
      info.executable_path = pathbuf;
    }
  }

  int mib[3] = {CTL_KERN, KERN_PROCARGS2, static_cast<int>(pid)};
  size_t buf_size = internal::kMaxKernProcArgs2BufferSize;
  std::vector<char> buffer(buf_size, 0);
  if (sysctl(mib, 3, buffer.data(), &buf_size, nullptr, 0) == 0 &&
      buf_size > 0 && buf_size <= buffer.size()) {
    if (std::optional<internal::ParsedKernProcArgs2> parsed =
            internal::ParseKernProcArgs2Buffer(
                base::span(buffer).first(buf_size))) {
      if (info.executable_path.empty() && !parsed->executable_path.empty()) {
        info.executable_path = std::move(parsed->executable_path);
      }
      info.argv = std::move(parsed->argv);
    }
  }
#endif

  if (!IsValidUserProcessId(info.parent_process_id) &&
      info.executable_path.empty() && info.argv.empty()) {
    return std::nullopt;
  }
  return info;
}

ProcessTree ExtractProcessTreeWithProvider(
    base::ProcessId start_pid,
    internal::ProcessInfoProvider provider) {
  ProcessTree tree;
  if (!IsValidUserProcessId(start_pid) || !provider) {
    return tree;
  }

  base::flat_set<base::ProcessId> visited_pids;
  base::ProcessId current_pid = start_pid;
  while (IsValidUserProcessId(current_pid) &&
         tree.process_nodes.size() < kMaxProcessAncestryDepth &&
         visited_pids.insert(current_pid).second) {
    std::optional<internal::RawProcessInfo> raw = provider.Run(current_pid);
    if (!raw.has_value()) {
      break;
    }

    std::string script_path =
        ExtractInterpreterScript(raw->executable_path, raw->argv);
    ProcessNode node(current_pid, raw->parent_process_id,
                     std::move(raw->executable_path), std::move(script_path));
    SanitizeProcessNodePaths(node);
    tree.process_nodes.push_back(std::move(node));

    const base::ProcessId ppid = raw->parent_process_id;
    if (!IsValidUserProcessId(ppid) || ppid == current_pid ||
        visited_pids.contains(ppid)) {
      break;
    }
    current_pid = ppid;
  }
  return tree;
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

namespace internal {

RawProcessInfo::RawProcessInfo() = default;
RawProcessInfo::RawProcessInfo(base::ProcessId parent_process_id,
                               std::string executable_path,
                               std::vector<std::string> argv)
    : parent_process_id(parent_process_id),
      executable_path(std::move(executable_path)),
      argv(std::move(argv)) {}
RawProcessInfo::RawProcessInfo(const RawProcessInfo&) = default;
RawProcessInfo& RawProcessInfo::operator=(const RawProcessInfo&) = default;
RawProcessInfo::RawProcessInfo(RawProcessInfo&&) noexcept = default;
RawProcessInfo& RawProcessInfo::operator=(RawProcessInfo&&) noexcept = default;
RawProcessInfo::~RawProcessInfo() = default;

std::optional<ParsedKernProcArgs2> ParseKernProcArgs2Buffer(
    base::span<const char> buffer) {
  if (buffer.size() < sizeof(int) ||
      buffer.size() > kMaxKernProcArgs2BufferSize) {
    return std::nullopt;
  }

  int argc = 0;
  base::byte_span_from_ref(argc).copy_from(
      base::as_bytes(buffer.first<sizeof(int)>()));
  if (argc <= 0) {
    return std::nullopt;
  }
  argc = std::min(argc, kMaxProcessArgCount);

  const base::span<const char> payload = buffer.subspan(sizeof(int));
  std::string_view view(payload.data(), payload.size());

  const size_t exec_end = view.find('\0');
  if (exec_end == std::string_view::npos || exec_end == 0) {
    return std::nullopt;
  }

  ParsedKernProcArgs2 result;
  result.executable_path = std::string(view.substr(0, exec_end));
  view.remove_prefix(exec_end);

  const size_t args_start = view.find_first_not_of('\0');
  if (args_start == std::string_view::npos) {
    return result;
  }
  view.remove_prefix(args_start);

  for (int i = 0; i < argc && !view.empty(); ++i) {
    const size_t arg_end = view.find('\0');
    if (arg_end == std::string_view::npos) {
      break;
    }
    result.argv.push_back(std::string(view.substr(0, arg_end)));
    view.remove_prefix(arg_end + 1);
  }

  return result;
}

ProcessTree ExtractProcessTreeWithProviderForTesting(
    base::ProcessId start_pid,
    ProcessInfoProvider provider) {
  return ExtractProcessTreeWithProvider(start_pid, std::move(provider));
}

}  // namespace internal

ProcessTree ExtractProcessTreeFromPid(base::ProcessId start_pid) {
  if (!IsValidUserProcessId(start_pid)) {
    return ProcessTree();
  }
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  return ExtractProcessTreeWithProvider(
      start_pid, base::BindRepeating(&QueryOsProcessInfo));
}

}  // namespace enterprise_connectors
