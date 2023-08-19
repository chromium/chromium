// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/updater_scope.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/util/util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/updater/tag.h"
#include "chrome/updater/util/win_util.h"
#endif

namespace updater {
namespace {

bool IsSystemProcessForCommandLine(const base::CommandLine& command_line) {
  return command_line.HasSwitch(kSystemSwitch);
}

}  // namespace

bool IsPrefersForCommandLine(const base::CommandLine& command_line) {
#if BUILDFLAG(IS_WIN)
  const absl::optional<tagging::TagArgs> tag_args =
      GetTagArgsForCommandLine(command_line).tag_args;
  return tag_args && !tag_args->apps.empty() &&
         tag_args->apps.front().needs_admin &&
         *tag_args->apps.front().needs_admin ==
             tagging::AppArgs::NeedsAdmin::kPrefers;
#else
  return false;
#endif
}

UpdaterScope GetUpdaterScopeForCommandLine(
    const base::CommandLine& command_line) {
#if BUILDFLAG(IS_WIN)
  if (IsSystemProcessForCommandLine(command_line)) {
    return UpdaterScope::kSystem;
  }

  // Assume only one app is present since bundles are not supported.
  const absl::optional<tagging::TagArgs> tag_args =
      GetTagArgsForCommandLine(command_line).tag_args;
  if (tag_args && !tag_args->apps.empty() &&
      tag_args->apps.front().needs_admin) {
    switch (*tag_args->apps.front().needs_admin) {
      case tagging::AppArgs::NeedsAdmin::kYes:
        return UpdaterScope::kSystem;
      case tagging::AppArgs::NeedsAdmin::kNo:
        return UpdaterScope::kUser;
      case tagging::AppArgs::NeedsAdmin::kPrefers:
        return command_line.HasSwitch(kCmdLinePrefersUser)
                   ? UpdaterScope::kUser
                   : UpdaterScope::kSystem;
    }
  }

  // The legacy updater could launch the shim without specifying the scope
  // explicitly. This includes command line switches: '/healthcheck', '/regsvc',
  // '/regserver', and '/ping'. In this case, choose system scope if this
  // program is run as a system shim.
  absl::optional<base::FilePath> system_shim_path =
      GetGoogleUpdateExePath(UpdaterScope::kSystem);
  base::FilePath exe_path;
  if (system_shim_path && base::PathService::Get(base::FILE_EXE, &exe_path) &&
      system_shim_path->DirName().IsParent(exe_path)) {
    return UpdaterScope::kSystem;
  }
  return UpdaterScope::kUser;
#else
  return IsSystemProcessForCommandLine(command_line) ? UpdaterScope::kSystem
                                                     : UpdaterScope::kUser;
#endif
}

UpdaterScope GetUpdaterScope() {
  return GetUpdaterScopeForCommandLine(*base::CommandLine::ForCurrentProcess());
}

bool IsSystemInstall() {
  return IsSystemInstall(GetUpdaterScope());
}

bool IsSystemInstall(UpdaterScope scope) {
  return scope == UpdaterScope::kSystem;
}

}  // namespace updater
