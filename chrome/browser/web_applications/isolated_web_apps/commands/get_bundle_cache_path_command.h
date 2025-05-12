// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_COMMANDS_GET_BUNDLE_CACHE_PATH_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_COMMANDS_GET_BUNDLE_CACHE_PATH_COMMAND_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "base/version.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"
#include "chrome/browser/web_applications/locks/app_lock.h"

namespace web_app {

class GetBundleCachePathSuccess {
 public:
  GetBundleCachePathSuccess(const base::FilePath& cached_bundle_path,
                            const base::Version& cached_version)
      : cached_bundle_path_(cached_bundle_path),
        cached_version_(cached_version) {}

  GetBundleCachePathSuccess(const GetBundleCachePathSuccess&) = default;
  ~GetBundleCachePathSuccess() = default;

  bool operator==(const GetBundleCachePathSuccess& other) const = default;

  const base::FilePath& cached_bundle_path() const {
    return cached_bundle_path_;
  }

  const base::Version& cached_version() const { return cached_version_; }

 private:
  base::FilePath cached_bundle_path_;
  base::Version cached_version_;
};

enum class GetBundleCachePathError {
  kSystemShutdown = 0,
  kProvidedVersionNotFound = 1,
  kIwaNotCached = 2,
};

std::string GetBundleCachePathErrorToString(GetBundleCachePathError error);

using GetBundleCachePathResult =
    base::expected<GetBundleCachePathSuccess, GetBundleCachePathError>;

// Gets IWA bundle path from cache if available. If `version` is provided,
// return the bundle with specified version or `kProvidedVersionNotFound` error.
// If `version` is empty, the function returns the bundle path with the newest
// cached version or `kIwaNotCached` error. This class takes `AppLock` for the
// bundle.
class GetBundleCachePathCommand
    : public WebAppCommand<AppLock, GetBundleCachePathResult> {
 public:
  using Callback = base::OnceCallback<void(GetBundleCachePathResult)>;

  GetBundleCachePathCommand(const IsolatedWebAppUrlInfo& url_info,
                            const std::optional<base::Version>& version,
                            IwaCacheClient::SessionType session_type,
                            Callback callback);
  GetBundleCachePathCommand(const GetBundleCachePathCommand&) = delete;
  GetBundleCachePathCommand& operator=(const GetBundleCachePathCommand&) =
      delete;
  ~GetBundleCachePathCommand() override;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;

 private:
  void CommandComplete(const GetBundleCachePathResult& result);

  std::unique_ptr<AppLock> lock_;
  const IsolatedWebAppUrlInfo url_info_;
  const std::optional<base::Version> version_;
  const IwaCacheClient::SessionType session_type_;

  base::WeakPtrFactory<GetBundleCachePathCommand> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_COMMANDS_GET_BUNDLE_CACHE_PATH_COMMAND_H_
