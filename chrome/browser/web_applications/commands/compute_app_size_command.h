// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_COMPUTE_APP_SIZE_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_COMPUTE_APP_SIZE_COMMAND_H_

#include <cstdint>
#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "chrome/browser/browsing_data/site_data_size_collector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "components/browsing_data/content/browsing_data_quota_helper.h"
#include "components/browsing_data/content/local_storage_helper.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/storage_usage_info.h"

namespace web_app {

class AppLock;
class AppLockDescription;
class LockDescription;
enum class Result;

// ComputeAppSizeCommand calculates the app and data size of a given app
class ComputeAppSizeCommand : public WebAppCommandTemplate<AppLock> {
 public:
  struct Size {
    uint64_t app_size_in_bytes = 0;
    uint64_t data_size_in_bytes = 0;
  };

  ComputeAppSizeCommand(
      const webapps::AppId& app_id,
      Profile* profile,
      base::OnceCallback<void(absl::optional<Size>)> callback);

  ~ComputeAppSizeCommand() override;

  // WebAppCommandTemplate<AppLock>:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;
  const LockDescription& lock_description() const override;
  base::Value ToDebugValue() const override;
  void OnShutdown() override;

 private:
  void OnGetIconSize(uint64_t size);
  void GetDataSize();
  void OnQuotaModelInfoLoaded(
      const SiteDataSizeCollector::QuotaStorageUsageInfoList&
          quota_storage_info_list);
  void GetSessionUsage();
  void OnLocalStorageModelInfoLoaded(
      const std::list<content::StorageUsageInfo>& local_storage_info_list);
  void ReportResultAndDestroy(CommandResult result);

  scoped_refptr<BrowsingDataQuotaHelper> quota_helper_;

  scoped_refptr<browsing_data::LocalStorageHelper> local_storage_helper_;

  AppLockDescription lock_description_;
  std::unique_ptr<AppLock> lock_;

  const webapps::AppId app_id_;
  const raw_ptr<Profile> profile_;
  base::OnceCallback<void(absl::optional<Size>)> callback_;
  url::Origin origin_;

  Size size_;

  base::WeakPtrFactory<ComputeAppSizeCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_COMPUTE_APP_SIZE_COMMAND_H_
