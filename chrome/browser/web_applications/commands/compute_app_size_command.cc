// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/compute_app_size_command.h"

#include <memory>
#include <utility>

#include "base/barrier_callback.h"
#include "base/debug/crash_logging.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/storage_usage_info.h"

namespace web_app {

ComputeAppSizeCommand::ComputeAppSizeCommand(
    const webapps::AppId& app_id,
    Profile* profile,
    base::OnceCallback<void(std::optional<ComputedAppSize>)> callback)
    : WebAppCommand<AppLock, std::optional<ComputedAppSize>>(
          "ComputeAppSizeCommand",
          AppLockDescription(app_id),
          std::move(callback),
          /*args_for_shutdown=*/
          ComputedAppSize()),
      app_id_(app_id),
      profile_(profile) {
  GetMutableDebugValue().Set("app_id", app_id);
}

ComputeAppSizeCommand::~ComputeAppSizeCommand() = default;

void ComputeAppSizeCommand::StartWithLock(std::unique_ptr<AppLock> lock) {
  lock_ = std::move(lock);

  if (!lock_->registrar().IsInstalled(app_id_)) {
    ReportResultAndDestroy(CommandResult::kFailure);
    return;
  }

  lock_->icon_manager().GetIconsSizeForApp(
      app_id_, base::BindOnce(&ComputeAppSizeCommand::OnGetIconSize,
                              weak_factory_.GetWeakPtr()));
}

void ComputeAppSizeCommand::OnGetIconSize(uint64_t icon_size) {
  size_.app_size_in_bytes = icon_size;
  GetMutableDebugValue().Set("app_size_in_bytes", base::ToString(icon_size));
  GetDataSize();
}

void ComputeAppSizeCommand::GetDataSize() {
  content::StoragePartition* storage_partition =
      profile_->GetDefaultStoragePartition();
  quota_helper_ = BrowsingDataQuotaHelper::Create(storage_partition);

  quota_helper_->StartFetching(
      base::BindOnce(&ComputeAppSizeCommand::OnQuotaModelInfoLoaded,
                     weak_factory_.GetWeakPtr()));
}

void ComputeAppSizeCommand::OnQuotaModelInfoLoaded(
    const SiteDataSizeCollector::QuotaStorageUsageInfoList&
        quota_storage_info_list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!lock_->registrar().IsInstalled(app_id_)) {
    // (crbug/1480755): This crash is not expected as the app is checked for
    // validity when the command is evoked in StartWithLock. We are also still
    // holding the lock so a change to the status of the app throughout is not
    // expected.
    NOTREACHED_IN_MIGRATION();
    ReportResultAndDestroy(CommandResult::kFailure);
    return;
  }

  GURL gurl = lock_->registrar().GetAppById(app_id_)->start_url();
  if (!gurl.is_valid()) {
    // (crbug/1480755): This crash is not expected as the app is checked for
    // validity when the command is evoked in StartWithLock. We are also still
    // holding the lock so a change to the status of the app throughout is not
    // expected.
    NOTREACHED_IN_MIGRATION();
    ReportResultAndDestroy(CommandResult::kFailure);
    return;
  }
  origin_ = url::Origin::Create(gurl);

  // TODO(crbug.com/40214522): Optimise the computation of the following loop.
  for (const auto& quota_info : quota_storage_info_list) {
    if (origin_ == quota_info.storage_key.origin()) {
      size_.data_size_in_bytes +=
          quota_info.temporary_usage + quota_info.syncable_usage;
    }
  }
  GetMutableDebugValue().Set("data_size_in_bytes",
                             base::ToString(size_.data_size_in_bytes));

  profile_->GetDefaultStoragePartition()
      ->GetDOMStorageContext()
      ->GetLocalStorageUsage(
          base::BindOnce(&ComputeAppSizeCommand::OnLocalStorageModelInfoLoaded,
                         weak_factory_.GetWeakPtr()));
}

void ComputeAppSizeCommand::OnLocalStorageModelInfoLoaded(
    const std::vector<content::StorageUsageInfo>& local_storage_info_list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (const auto& local_storage_info : local_storage_info_list) {
    url::Origin local_origin = local_storage_info.storage_key.origin();
    if (origin_ == local_origin) {
      size_.data_size_in_bytes += local_storage_info.total_size_bytes;
    }
  }

  ReportResultAndDestroy(CommandResult::kSuccess);
}

void ComputeAppSizeCommand::ReportResultAndDestroy(CommandResult result) {
  CompleteAndSelfDestruct(result, result == CommandResult::kSuccess
                                      ? std::move(size_)
                                      : ComputedAppSize());
}

}  // namespace web_app
