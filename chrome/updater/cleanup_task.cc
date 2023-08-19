// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/cleanup_task.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/sequence_checker.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/updater/app/app_uninstall.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/updater/util/win_util.h"
#endif

namespace updater {

namespace {

// Versions up to this version will be uninstalled.
constexpr char kCleanupVersionMax[] = "117.0.5859.0";

void CleanupGoogleUpdate(UpdaterScope scope) {
#if BUILDFLAG(IS_WIN)
  // Delete anything other than `GoogleUpdate.exe` under `\Google\Update`.
  bool deleted = DeleteExcept(GetGoogleUpdateExePath(scope));
  VLOG_IF(1, !deleted) << "Failed to delete obsolete files near "
                       << GetGoogleUpdateExePath(scope);
#endif  // BUILDFLAG(IS_WIN)
}

void CleanupOldUpdaterVersions(UpdaterScope scope) {
  CHECK(base::Version(kCleanupVersionMax).IsValid());
  CHECK(base::Version(kUpdaterVersion)
            .CompareTo(base::Version(kCleanupVersionMax)) > 0);
  absl::optional<base::FilePath> dir = GetInstallDirectory(scope);
  if (!dir) {
    return;
  }
  base::FileEnumerator(*dir, false, base::FileEnumerator::DIRECTORIES)
      .ForEach([&scope](const base::FilePath& item) {
        base::Version version(item.BaseName().MaybeAsASCII());
        if (!version.IsValid() ||
            version.CompareTo(base::Version(kCleanupVersionMax)) > 0) {
          return;
        }
        VLOG(1) << __func__ << " cleaning up " << item;

        // Attempt a normal uninstall.
        const base::FilePath version_executable_path =
            item.Append(GetExecutableRelativePath());
        if (base::PathExists(version_executable_path)) {
          base::LaunchProcess(
              GetUninstallSelfCommandLine(scope, version_executable_path), {})
              .WaitForExitWithTimeout(base::Minutes(5), nullptr);
        }

        // Recursively delete the directory in case uninstall fails.
        base::DeletePathRecursively(item);
      });
}

}  // namespace

CleanupTask::CleanupTask(UpdaterScope scope) : scope_(scope) {}

CleanupTask::~CleanupTask() = default;

void CleanupTask::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::WithBaseSyncPrimitives()},
      base::BindOnce(
          [](UpdaterScope scope) {
            CleanupGoogleUpdate(scope);
            CleanupOldUpdaterVersions(scope);
          },
          scope_),
      std::move(callback));
}

}  // namespace updater
