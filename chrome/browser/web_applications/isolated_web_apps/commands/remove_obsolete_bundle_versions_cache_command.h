// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_COMMANDS_REMOVE_OBSOLETE_BUNDLE_VERSIONS_CACHE_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_COMMANDS_REMOVE_OBSOLETE_BUNDLE_VERSIONS_CACHE_COMMAND_H_

#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"

namespace web_app {

class RemoveObsoleteBundleVersionsSuccess {
 public:
  explicit RemoveObsoleteBundleVersionsSuccess(
      size_t number_of_removed_versions)
      : number_of_removed_versions_(number_of_removed_versions) {}

  RemoveObsoleteBundleVersionsSuccess(
      const RemoveObsoleteBundleVersionsSuccess& other) = default;
  ~RemoveObsoleteBundleVersionsSuccess() = default;

  bool operator==(const RemoveObsoleteBundleVersionsSuccess& other) const =
      default;

  size_t number_of_removed_versions() const {
    return number_of_removed_versions_;
  }

  std::string ToString() const;

 private:
  size_t number_of_removed_versions_;
};

class RemoveObsoleteBundleVersionsError {
 public:
  // These are used in histograms, do not remove/renumber entries. If you're
  // adding to this enum with the intention that it will be logged, update the
  // `IsolatedWebAppRemoveObsoleteBundleVersionsError` enum listing in
  // tools/metrics/histograms/metadata/webapps/enums.xml.
  enum class Type {
    kSystemShutdown = 0,
    kAppNotInstalled = 1,
    kInstalledVersionNotCached = 2,
    kCouldNotDeleteAllVersions = 3,
    kMaxValue = kCouldNotDeleteAllVersions,
  };

  explicit RemoveObsoleteBundleVersionsError(
      Type type,
      size_t number_of_failed_remove_versions = 0)
      : type_(type),
        number_of_failed_to_remove_versions_(number_of_failed_remove_versions) {
  }

  RemoveObsoleteBundleVersionsError(
      const RemoveObsoleteBundleVersionsError& other) = default;
  ~RemoveObsoleteBundleVersionsError() = default;

  bool operator==(const RemoveObsoleteBundleVersionsError& other) const =
      default;

  Type type() const { return type_; }

  size_t number_of_failed_to_remove_versions() const {
    return number_of_failed_to_remove_versions_;
  }

  std::string ToString() const;

 private:
  Type type_;
  // Valid only for `kCouldNotDeleteAllVersions` failure.
  size_t number_of_failed_to_remove_versions_;
};

using RemoveObsoleteBundleVersionsResult =
    base::expected<RemoveObsoleteBundleVersionsSuccess,
                   RemoveObsoleteBundleVersionsError>;

// Keeps only currently installed version bundle in cache for provided IWA and
// removes all other bundle versions. If the installed version is not cached,
// returns `kInstalledVersionNotCached` and does not remove any other version in
// cache. Takes `AppLock` for the IWA to avoid race condition with other bundle
// cache commands.
class RemoveObsoleteBundleVersionsCacheCommand
    : public WebAppCommand<AppLock, RemoveObsoleteBundleVersionsResult> {
 public:
  using Callback = base::OnceCallback<void(RemoveObsoleteBundleVersionsResult)>;

  RemoveObsoleteBundleVersionsCacheCommand(
      const IsolatedWebAppUrlInfo& url_info,
      IwaCacheClient::SessionType session_type,
      Callback callback);
  RemoveObsoleteBundleVersionsCacheCommand(
      const RemoveObsoleteBundleVersionsCacheCommand&) = delete;
  RemoveObsoleteBundleVersionsCacheCommand& operator=(
      const RemoveObsoleteBundleVersionsCacheCommand&) = delete;

  ~RemoveObsoleteBundleVersionsCacheCommand() override;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;

 private:
  void CommandComplete(const RemoveObsoleteBundleVersionsResult& result);

  std::unique_ptr<AppLock> lock_;
  const IsolatedWebAppUrlInfo url_info_;
  const IwaCacheClient::SessionType session_type_;

  base::WeakPtrFactory<RemoveObsoleteBundleVersionsCacheCommand>
      weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_COMMANDS_REMOVE_OBSOLETE_BUNDLE_VERSIONS_CACHE_COMMAND_H_
