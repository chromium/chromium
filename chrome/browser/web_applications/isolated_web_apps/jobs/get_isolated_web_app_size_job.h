// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_JOBS_GET_ISOLATED_WEB_APP_SIZE_JOB_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_JOBS_GET_ISOLATED_WEB_APP_SIZE_JOB_H_

#include <cstdint>
#include <optional>

#include "base/functional/callback.h"
#include "base/functional/concurrent_closures.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/command_result.h"
#include "components/webapps/common/web_app_id.h"
#include "url/origin.h"

class Profile;

namespace web_app {

class ComputedAppSizeWithOrigin;
class WithAppResources;

// Calculates the total on-disk storage size for a give installed isolated web
// app, including both the web app's web platform storage as well as Chrome's
// internal storage of things like icons etc., including bundle size.
class GetIsolatedWebAppSizeJob {
 public:
  using ResultCallback =
      base::OnceCallback<void(std::optional<ComputedAppSizeWithOrigin> result)>;

  GetIsolatedWebAppSizeJob(Profile* profile,
                           const webapps::AppId& app_id,
                           base::Value::Dict& debug_value,
                           ResultCallback result_callback);
  ~GetIsolatedWebAppSizeJob();

  void Start(WithAppResources* lock_with_app_resources);

 private:
  void OnGetIconStorageUsage(uint64_t size);
  void StoragePartitionSizeFetched(int64_t size);
  void OnBundleSizeComputed(std::optional<int64_t> bundle_size);
  void CompleteJobWithError();
  void CompleteJob();

  const webapps::AppId app_id_;
  url::Origin iwa_origin_;
  uint64_t browsing_data_size_ = 0u;
  std::optional<uint64_t> bundle_size_;
  size_t icon_size_ = 0u;
  const raw_ptr<Profile> profile_;
  raw_ptr<WithAppResources> lock_with_app_resources_ = nullptr;
  const raw_ref<base::Value::Dict> debug_value_;
  ResultCallback result_callback_;
  base::WeakPtrFactory<GetIsolatedWebAppSizeJob> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_JOBS_GET_ISOLATED_WEB_APP_SIZE_JOB_H_
