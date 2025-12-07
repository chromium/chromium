// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/get_progressive_web_app_size_job.h"

#include "chrome/browser/browsing_data/chrome_browsing_data_model_delegate.h"
#include "chrome/browser/web_applications/commands/computed_app_size.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

GetProgressiveWebAppSizeJob::GetProgressiveWebAppSizeJob(
    Profile* profile,
    const webapps::AppId& app_id,
    base::Value::Dict& debug_value,
    ResultCallback result_callback)
    : app_id_(app_id),
      profile_(profile),
      debug_value_(debug_value),
      result_callback_(std::move(result_callback)) {
  debug_value_->Set("profile", profile->GetDebugName());
}

GetProgressiveWebAppSizeJob::~GetProgressiveWebAppSizeJob() = default;

void GetProgressiveWebAppSizeJob::Start(
    WithAppResources* lock_with_app_resources) {
  CHECK(lock_with_app_resources);
  lock_with_app_resources_ = lock_with_app_resources;
  lock_with_app_resources_->icon_manager().GetIconsSizeForApp(
      app_id_,
      base::BindOnce(&GetProgressiveWebAppSizeJob::OnGetIconStorageUsage,
                     weak_factory_.GetWeakPtr()));
}

void GetProgressiveWebAppSizeJob::OnGetIconStorageUsage(uint64_t icon_size) {
  icon_size_ = icon_size;
  debug_value_->Set("app_size_in_bytes", base::ToString(icon_size));
  GetDataSize();
}

void GetProgressiveWebAppSizeJob::GetDataSize() {
  content::StoragePartition* storage_partition =
      profile_->GetDefaultStoragePartition();
  quota_helper_ = BrowsingDataQuotaHelper::Create(storage_partition);

  quota_helper_->StartFetching(
      base::BindOnce(&GetProgressiveWebAppSizeJob::OnQuotaModelInfoLoaded,
                     weak_factory_.GetWeakPtr()));
}

void GetProgressiveWebAppSizeJob::OnQuotaModelInfoLoaded(
    const SiteDataSizeCollector::QuotaStorageUsageInfoList&
        quota_storage_info_list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!lock_with_app_resources_->registrar().IsInRegistrar(app_id_)) {
    // (crbug.com/1480755): This crash is not expected as the app is checked for
    // validity when the command is evoked in StartWithLock. We are also still
    // holding the lock so a change to the status of the app throughout is not
    // expected.
    NOTREACHED();
  }

  GURL gurl =
      lock_with_app_resources_->registrar().GetAppById(app_id_)->start_url();
  if (!gurl.is_valid()) {
    // (crbug.com/1480755): This crash is not expected as the app is checked for
    // validity when the command is evoked in StartWithLock. We are also still
    // holding the lock so a change to the status of the app throughout is not
    // expected.
    NOTREACHED();
  }
  origin_ = url::Origin::Create(gurl);

  // TODO(crbug.com/40214522): Optimise the computation of the following loop.
  for (const auto& quota_info : quota_storage_info_list) {
    if (origin_ == quota_info.storage_key.origin()) {
      browsing_data_size_ += quota_info.usage;
    }
  }
  debug_value_->Set("data_size_in_bytes", base::ToString(browsing_data_size_));

  profile_->GetDefaultStoragePartition()
      ->GetDOMStorageContext()
      ->GetLocalStorageUsage(base::BindOnce(
          &GetProgressiveWebAppSizeJob::OnLocalStorageModelInfoLoaded,
          weak_factory_.GetWeakPtr()));
}

void GetProgressiveWebAppSizeJob::OnLocalStorageModelInfoLoaded(
    const std::vector<content::StorageUsageInfo>& local_storage_info_list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  for (const auto& local_storage_info : local_storage_info_list) {
    url::Origin local_origin = local_storage_info.storage_key.origin();
    if (origin_ == local_origin) {
      browsing_data_size_ += local_storage_info.total_size_bytes;
    }
  }
  MaybeReturnSize();
}

void GetProgressiveWebAppSizeJob::MaybeReturnSize() {
  if (browsing_data_size_ == 0u && icon_size_ == 0u) {
    CompleteJobWithError();
    return;
  }

  std::optional<web_app::ComputedAppSizeWithOrigin> proxy =
      web_app::ComputedAppSizeWithOrigin(icon_size_, browsing_data_size_,
                                         std::nullopt);

  std::move(result_callback_).Run(proxy);
}

void GetProgressiveWebAppSizeJob::CompleteJobWithError() {
  std::move(result_callback_).Run(/*result=*/std::nullopt);
}

}  // namespace web_app
