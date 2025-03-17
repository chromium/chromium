// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_JOBS_GET_PROGRESSIVE_WEB_APP_SIZE_JOB_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_JOBS_GET_PROGRESSIVE_WEB_APP_SIZE_JOB_H_

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/browsing_data/site_data_size_collector.h"
#include "chrome/browser/web_applications/web_app.h"
#include "components/webapps/common/web_app_id.h"
#include "url/origin.h"

class Profile;

namespace content {
struct StorageUsageInfo;
}

namespace web_app {

class WithAppResources;
class ComputedAppSizeWithOrigin;

// Calculates the total on-disk storage size for a give installed web app,
// including both the web app's web platform storage as well as Chrome's
// internal storage of things like icons.
class GetProgressiveWebAppSizeJob {
 public:
  using ResultCallback =
      base::OnceCallback<void(std::optional<ComputedAppSizeWithOrigin> result)>;

  GetProgressiveWebAppSizeJob(Profile* profile,
                              const webapps::AppId& app_id,
                              base::Value::Dict& debug_value,
                              ResultCallback result_callback);
  ~GetProgressiveWebAppSizeJob();

  void Start(WithAppResources* lock_with_app_resources);

 private:
  void MaybeReturnSize();
  void CompleteJobWithError();
  void OnGetIconStorageUsage(uint64_t size);
  void GetDataSize();
  void OnQuotaModelInfoLoaded(
      const SiteDataSizeCollector::QuotaStorageUsageInfoList&
          quota_storage_info_list);
  void OnLocalStorageModelInfoLoaded(
      const std::vector<content::StorageUsageInfo>& local_storage_info_list);

  const webapps::AppId app_id_;
  uint64_t browsing_data_size_ = 0u;
  uint64_t icon_size_ = 0u;
  const raw_ptr<Profile> profile_;
  raw_ptr<WithAppResources> lock_with_app_resources_ = nullptr;
  const raw_ref<base::Value::Dict> debug_value_;
  ResultCallback result_callback_;
  scoped_refptr<BrowsingDataQuotaHelper> quota_helper_;
  url::Origin origin_;

  base::WeakPtrFactory<GetProgressiveWebAppSizeJob> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_JOBS_GET_PROGRESSIVE_WEB_APP_SIZE_JOB_H_
