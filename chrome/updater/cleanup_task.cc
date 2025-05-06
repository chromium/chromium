// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/cleanup_task.h"

#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/sequence_checker.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/updater/app/app_uninstall.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/util.h"
#include "components/update_client/crx_cache.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/updater/util/win_util.h"
#endif

namespace updater {
namespace {

constexpr int kMilestoneDeletionThreshold = 8;

void CleanupGoogleUpdate(UpdaterScope scope) {
#if BUILDFLAG(IS_WIN)
  // Delete anything other than `GoogleUpdate.exe` under `\Google\Update`.
  bool deleted = DeleteExcept(GetGoogleUpdateExePath(scope));
  VLOG_IF(1, !deleted) << "Failed to delete obsolete files near "
                       << GetGoogleUpdateExePath(scope);
#endif  // BUILDFLAG(IS_WIN)
}

void CleanupOldUpdaterVersions(UpdaterScope scope) {
  base::Version cleanup_max =
      base::Version({base::Version(kUpdaterVersion).components()[0] -
                     kMilestoneDeletionThreshold});
  CHECK_GT(base::Version(kUpdaterVersion), cleanup_max);
  std::optional<base::FilePath> dir = GetInstallDirectory(scope);
  if (!dir) {
    return;
  }
  base::FileEnumerator(*dir, false, base::FileEnumerator::DIRECTORIES)
      .ForEach([&scope, &cleanup_max](const base::FilePath& item) {
        base::Version version(item.BaseName().AsUTF8Unsafe());
        if (!version.IsValid() || version.CompareTo(cleanup_max) > 0) {
          return;
        }
        VLOG(1) << __func__ << " cleaning up " << item;

        // Attempt a normal uninstall.
        const base::Process process = base::LaunchProcess(
            GetUninstallSelfCommandLine(
                scope, item.Append(GetExecutableRelativePath())),
            {});
        if (process.IsValid()) {
          process.WaitForExitWithTimeout(base::Minutes(5), nullptr);
        }

        // Recursively delete the directory in case uninstall fails.
        base::DeletePathRecursively(item);
      });
}

}  // namespace

CleanupTask::CleanupTask(UpdaterScope scope, scoped_refptr<Configurator> config)
    : scope_(scope), config_(config) {}

CleanupTask::~CleanupTask() = default;

void CleanupTask::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::WithBaseSyncPrimitives()},
      base::BindOnce(
          [](UpdaterScope scope) {
            CleanupGoogleUpdate(scope);
            CleanupOldUpdaterVersions(scope);
#if BUILDFLAG(IS_MAC)
            // TODO(crbug.com/394302692): Delete after M140.
            CleanOldCrxCache();
#endif  // IS_MAC
          },
          scope_),
      base::BindOnce(
          [](scoped_refptr<Configurator> config, base::OnceClosure callback) {
            config->GetCrxCache()->RemoveIfNot(
                config->GetUpdaterPersistedData()->GetAppIds(),
                std::move(callback));
          },
          config_, std::move(callback)));
}

}  // namespace updater
