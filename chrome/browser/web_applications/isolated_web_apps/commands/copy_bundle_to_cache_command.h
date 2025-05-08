// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_COMMANDS_COPY_BUNDLE_TO_CACHE_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_COMMANDS_COPY_BUNDLE_TO_CACHE_COMMAND_H_

#include "base/types/expected.h"
#include "base/version.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"

namespace web_app {

class CopyBundleToCacheSuccess {
 public:
  explicit CopyBundleToCacheSuccess(const base::FilePath& cached_bundle_path)
      : cached_bundle_path_(cached_bundle_path) {}

  CopyBundleToCacheSuccess(const CopyBundleToCacheSuccess& other) = default;
  ~CopyBundleToCacheSuccess() = default;

  bool operator==(const CopyBundleToCacheSuccess& other) const = default;

  const base::FilePath& cached_bundle_path() const {
    return cached_bundle_path_;
  }

 private:
  base::FilePath cached_bundle_path_;
};

enum class CopyBundleToCacheError {
  kSystemShutdown,
  kAppNotInstalled,
  kNotIwa,
  kCannotExtractOwnedBundlePath,
  kFailedToCreateDir,
  kFailedToCopyFile,
};

std::string CopyBundleToCacheErrorToString(CopyBundleToCacheError error);

using CopyBundleToCacheResult =
    base::expected<CopyBundleToCacheSuccess, CopyBundleToCacheError>;

// Copies IWA bundle file to the cache. To prevent race conditions with other
// cache operations, this class takes `AppLock` for the bundle.
class CopyBundleToCacheCommand
    : public WebAppCommand<AppLock, CopyBundleToCacheResult> {
 public:
  using Callback = base::OnceCallback<void(CopyBundleToCacheResult)>;

  CopyBundleToCacheCommand(const IsolatedWebAppUrlInfo& url_info,
                           IwaCacheClient::SessionType session_type,
                           Profile& profile,
                           Callback callback);
  CopyBundleToCacheCommand(const CopyBundleToCacheCommand&) = delete;
  CopyBundleToCacheCommand& operator=(const CopyBundleToCacheCommand&) = delete;

  ~CopyBundleToCacheCommand() override;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;

 private:
  void CommandComplete(const CopyBundleToCacheResult& result);

  std::unique_ptr<AppLock> lock_;
  const IsolatedWebAppUrlInfo url_info_;
  const IwaCacheClient::SessionType session_type_;
  const raw_ref<Profile> profile_;

  base::WeakPtrFactory<CopyBundleToCacheCommand> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_COMMANDS_COPY_BUNDLE_TO_CACHE_COMMAND_H_
