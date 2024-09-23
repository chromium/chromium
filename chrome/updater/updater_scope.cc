// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/updater_scope.h"

#include <optional>

#include "base/command_line.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/util/util.h"

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

std::optional<tagging::NeedsAdmin> NeedsAdminFromTagArgs(
    const std::optional<tagging::TagArgs> tag_args) {
  if (!tag_args) {
    return {};
  }
  if (!tag_args->apps.empty()) {
    return tag_args->apps.front().needs_admin;
  }
  if (tag_args->runtime_mode) {
    return tag_args->runtime_mode->needs_admin;
  }
  return {};
}

bool IsPrefersForCommandLine(const base::CommandLine& command_line) {
#if BUILDFLAG(IS_WIN)
  std::optional<tagging::NeedsAdmin> needs_admin =
      NeedsAdminFromTagArgs(GetTagArgsForCommandLine(command_line).tag_args);
  return needs_admin ? *needs_admin == tagging::NeedsAdmin::kPrefers : false;
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
  std::optional<tagging::NeedsAdmin> needs_admin =
      NeedsAdminFromTagArgs(GetTagArgsForCommandLine(command_line).tag_args);
  if (needs_admin) {
    switch (*needs_admin) {
      case tagging::NeedsAdmin::kYes:
        return UpdaterScope::kSystem;
      case tagging::NeedsAdmin::kNo:
        return UpdaterScope::kUser;
      case tagging::NeedsAdmin::kPrefers:
        return command_line.HasSwitch(kCmdLinePrefersUser)
                   ? UpdaterScope::kUser
                   : UpdaterScope::kSystem;
    }
  }

  // The legacy updater could launch the shim without specifying the scope
  // explicitly. This includes command line switches: '/healthcheck', '/regsvc',
  // '/regserver', and '/ping'. In this case, choose system scope if this
  // program is run as a system shim.
  std::optional<base::FilePath> system_shim_path =
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
