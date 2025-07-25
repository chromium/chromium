// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_COMMANDS_CLEANUP_BUNDLE_CACHE_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_COMMANDS_CLEANUP_BUNDLE_CACHE_COMMAND_H_

#include "base/types/expected.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"

namespace web_app {

// Represents a successful cleanup of the bundle cache.
class CleanupBundleCacheSuccess {
 public:
  explicit CleanupBundleCacheSuccess(size_t number_of_cleaned_up_directories)
      : number_of_cleaned_up_directories_(number_of_cleaned_up_directories) {}

  CleanupBundleCacheSuccess(const CleanupBundleCacheSuccess& other) = default;
  ~CleanupBundleCacheSuccess() = default;

  bool operator==(const CleanupBundleCacheSuccess& other) const = default;

  size_t number_of_cleaned_up_directories() const {
    return number_of_cleaned_up_directories_;
  }

  std::string ToString() const;

 private:
  size_t number_of_cleaned_up_directories_ = 0;
};

// Represents an error during bundle cache cleanup.
class CleanupBundleCacheError {
 public:
  // These are used in histograms, do not remove/renumber entries. If you're
  // adding to this enum with the intention that it will be logged, update the
  // `IsolatedWebAppCleanupBundleCacheError` enum listing in
  // tools/metrics/histograms/metadata/webapps/enums.xml.
  enum class Type {
    kSystemShutdown = 0,
    kCouldNotDeleteAllBundles = 1,
    kMaxValue = kCouldNotDeleteAllBundles
  };

  explicit CleanupBundleCacheError(
      Type type,
      size_t number_of_failed_to_cleaned_up_directories = 0)
      : type_(type),
        number_of_failed_to_cleaned_up_directories_(
            number_of_failed_to_cleaned_up_directories) {}

  CleanupBundleCacheError(const CleanupBundleCacheError& other) = default;
  ~CleanupBundleCacheError() = default;

  bool operator==(const CleanupBundleCacheError& other) const = default;

  Type type() const { return type_; }

  size_t number_of_failed_to_cleaned_up_directories() const {
    return number_of_failed_to_cleaned_up_directories_;
  }

  std::string ToString() const;

 private:
  Type type_;
  // Valid only for `kCouldNotDeleteAllBundles` failure.
  size_t number_of_failed_to_cleaned_up_directories_ = 0;
};

using CleanupBundleCacheResult =
    base::expected<CleanupBundleCacheSuccess, CleanupBundleCacheError>;

// Cleans all IWA cached bundles for `session_type` which are not in the
// `iwas_to_keep_in_cache`. During the cleanup, this class iterates through all
// cached directories for Managed Guest Session or kiosk. To avoid adding new
// directories during the iteration, this class takes `AllAppsLock`.
class CleanupBundleCacheCommand
    : public WebAppCommand<AllAppsLock, CleanupBundleCacheResult> {
 public:
  using Callback = base::OnceCallback<void(CleanupBundleCacheResult)>;

  CleanupBundleCacheCommand(
      const std::vector<web_package::SignedWebBundleId>& iwas_to_keep_in_cache,
      IwaCacheClient::SessionType session_type,
      Callback callback);
  CleanupBundleCacheCommand(const CleanupBundleCacheCommand&) = delete;
  CleanupBundleCacheCommand& operator=(const CleanupBundleCacheCommand&) =
      delete;

  ~CleanupBundleCacheCommand() override;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AllAppsLock> lock) override;

 private:
  void CommandComplete(const CleanupBundleCacheResult& result);

  std::unique_ptr<AllAppsLock> lock_;
  const std::vector<web_package::SignedWebBundleId> iwas_to_keep_in_cache_;
  const IwaCacheClient::SessionType session_type_;

  base::WeakPtrFactory<CleanupBundleCacheCommand> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_COMMANDS_CLEANUP_BUNDLE_CACHE_COMMAND_H_
