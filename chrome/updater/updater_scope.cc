// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/updater_scope.h"

#include "base/check_op.h"
#include "base/command_line.h"
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

  // TODO(crbug.com/1128631): support bundles. For now, assume one app.
  const absl::optional<tagging::TagArgs> tag_args =
      GetTagArgsForCommandLine(command_line).tag_args;
  if (tag_args && !tag_args->apps.empty() &&
      tag_args->apps.front().needs_admin) {
    DCHECK_EQ(tag_args->apps.size(), size_t{1});
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

  // crbug.com(1214058): consider handling the elevation case by
  // calling IsUserAdmin().
  return UpdaterScope::kUser;
#else
  return IsSystemProcessForCommandLine(command_line) ? UpdaterScope::kSystem
                                                     : UpdaterScope::kUser;
#endif
}

UpdaterScope GetUpdaterScope() {
  return GetUpdaterScopeForCommandLine(GetCommandLineLegacyCompatible());
}

bool IsSystemInstall() {
  return IsSystemInstall(GetUpdaterScope());
}

bool IsSystemInstall(UpdaterScope scope) {
  return scope == UpdaterScope::kSystem;
}

}  // namespace updater
