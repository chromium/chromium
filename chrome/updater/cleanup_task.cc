// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/cleanup_task.h"

#include <optional>

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
#include "chrome/updater/app/app_uninstall.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/util.h"

#if BUILDFLAG(IS_WIN)
#include <string>
#include <vector>

#include "base/strings/sys_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/win_constants.h"
#endif

namespace updater {
namespace {

constexpr int kMilestoneDeletionThreshold = 8;

#if BUILDFLAG(IS_WIN)
// TODO(crbug.com/335673799) - remove the code in M129, after the brand codes
// have been repaired.
void RepairAppBrandCode(UpdaterScope scope,
                        scoped_refptr<PersistedData> persisted_data) {
  const HKEY root = UpdaterScopeToHKeyRoot(scope);

  struct AppBrand {
    std::string app_id;
    std::string brand;
  };

  // Contains the pairs of {`app_id`, `brand`}, if the `brand` is present.
  const std::vector<AppBrand> app_brands = [persisted_data] {
    std::vector<AppBrand> app_brands;
    for (const std::string& app_id : persisted_data->GetAppIds()) {
      const std::string brand = persisted_data->GetBrandCode(app_id);
      if (brand.empty()) {
        continue;
      }
      app_brands.emplace_back(app_id, brand);
    }
    return app_brands;
  }();

  // Updates the brand in registry, if a `brand` is present in prefs and the
  // the client state for the `app_id` does not contain a `brand`.
  for (const auto& [app_id, brand] : app_brands) {
    const std::wstring brand_prefs = base::SysUTF8ToWide(brand);
    if (brand_prefs.empty()) {
      continue;
    }
    base::win::RegKey key;
    if (key.Open(root, GetAppClientStateKey(app_id).c_str(),
                 Wow6432(KEY_READ | KEY_WRITE)) != ERROR_SUCCESS) {
      continue;
    }
    std::wstring brand_registry;
    key.ReadValue(kRegValueBrandCode, &brand_registry);
    if (!brand_registry.empty()) {
      continue;
    }
    VLOG(1) << __func__ << ": missing " << brand_prefs << " for " << app_id;
    const LONG res = key.WriteValue(kRegValueBrandCode, brand_prefs.c_str());
    VLOG(1) << __func__ << [&res] {
      return res != ERROR_SUCCESS ? ": not repaired " : ": repaired ";
    }() << res;
  }
}
#endif  // IS_WIN

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
        base::Version version(item.BaseName().MaybeAsASCII());
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

#if BUILDFLAG(IS_WIN)
  if (config_) {
    RepairAppBrandCode(scope_, config_->GetUpdaterPersistedData());
  }
#endif  // IS_WIN

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
