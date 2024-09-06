// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLATION_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLATION_MANAGER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/files/scoped_temp_file.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"

namespace base {
class CommandLine;
enum class TaskPriority : uint8_t;
}  // namespace base

class Profile;

namespace web_app {

class IsolatedWebAppUrlInfo;
class WebAppProvider;

// This class manages Installation related operations for Isolated Web App.
//
// The `InstallFromCommandLine` method can be used to imperatively parse the
// provided command line and install an IWA if specified.
//
// on `Start()`, `MaybeScheduleGarbageCollection()` will check pref values to
// determine whether to schedule a `GarbageCollectStoragePartitionCommand`.
//
// On ChromeOS only, the command line will be parsed whenever a new manager is
// started, which occurs on `Profile` initialization. This is done this way
// because the browser does not go through the "normal" startup flow on
// ChromeOS, and has different startup behaviors depending on whether or not Ash
// or Lacros is used.
//
// TODO(cmfcmf): Revisit this behavior once using Ash instead of Lacros is no
// longer possible.
class IsolatedWebAppInstallationManager {
 public:
  using MaybeInstallIsolatedWebAppCommandSuccess =
      base::expected<InstallIsolatedWebAppCommandSuccess, std::string>;
  using MaybeIwaInstallSource =
      base::expected<std::optional<IsolatedWebAppInstallSource>, std::string>;

  explicit IsolatedWebAppInstallationManager(Profile& profile);
  ~IsolatedWebAppInstallationManager();

  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);

  void Start();

  enum class InstallSurface {
    kDevUi,
  };

  void InstallIsolatedWebAppFromDevModeProxy(
      const GURL& gurl,
      InstallSurface install_surface,
      base::OnceCallback<void(MaybeInstallIsolatedWebAppCommandSuccess)>
          callback);

  void InstallIsolatedWebAppFromDevModeBundle(
      const base::FilePath& path,
      InstallSurface install_surface,
      base::OnceCallback<void(MaybeInstallIsolatedWebAppCommandSuccess)>
          callback);

  void InstallIsolatedWebAppFromDevModeBundle(
      const base::ScopedTempFile* file,
      InstallSurface install_surface,
      base::OnceCallback<void(MaybeInstallIsolatedWebAppCommandSuccess)>
          callback);

  void OnReportInstallationResultForTesting(
      base::RepeatingCallback<void(MaybeInstallIsolatedWebAppCommandSuccess)>
          on_report_installation_result) {
    on_report_installation_result_ = std::move(on_report_installation_result);
  }

  static bool HasIwaInstallSwitch(const base::CommandLine& command_line);

  // Attempts to install an IWA if the respective command line parameters are
  // provided. It might silently fail for multiple reasons, such as:
  // - missing command line parameters
  // - missing `WebAppProvider`
  // - browser shutting down
  static void MaybeInstallIwaFromCommandLine(
      const base::CommandLine& command_line,
      Profile& profile);

  static void GetIsolatedWebAppInstallSourceFromCommandLine(
      const base::CommandLine& command_line,
      base::OnceCallback<void(MaybeIwaInstallSource)> callback);

  const base::OneShotEvent&
  on_garbage_collect_storage_partitions_done_for_testing() {
    return on_garbage_collect_storage_partitions_done_for_testing_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(IsolatedWebAppInstallationManagerTest,
                           NoInstallationWhenFeatureDisabled);
  FRIEND_TEST_ALL_PREFIXES(IsolatedWebAppInstallationManagerTest,
                           NoInstallationWhenDevModeFeatureDisabled);
  FRIEND_TEST_ALL_PREFIXES(IsolatedWebAppInstallationManagerTest,
                           NoInstallationWhenDevModePolicyDisabled);

  static IsolatedWebAppInstallSource CreateInstallSource(
      absl::variant<base::FilePath, const base::ScopedTempFile*, url::Origin>
          source,
      InstallSurface surface);

  // Install an IWA from command line, if the command line specifies the
  // appropriate switches.
  void InstallFromCommandLine(
      const base::CommandLine& command_line,
      std::unique_ptr<ScopedKeepAlive> keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
      base::TaskPriority task_priority);

  void InstallIsolatedWebAppFromInstallSource(
      MaybeIwaInstallSource install_source,
      base::OnceCallback<void(MaybeInstallIsolatedWebAppCommandSuccess)>
          callback);

  void InstallIsolatedWebAppFromInstallSource(
      std::unique_ptr<ScopedKeepAlive> keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
      MaybeIwaInstallSource install_source,
      base::OnceCallback<void(MaybeInstallIsolatedWebAppCommandSuccess)>
          callback);

  void OnGetIsolatedWebAppInstallSourceFromCommandLine(
      std::unique_ptr<ScopedKeepAlive> keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
      MaybeIwaInstallSource install_source);

  void OnGetIsolatedWebAppUrlInfo(
      std::unique_ptr<ScopedKeepAlive> keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
      const IsolatedWebAppInstallSource& install_source,
      base::OnceCallback<void(MaybeInstallIsolatedWebAppCommandSuccess)>
          callback,
      base::expected<IsolatedWebAppUrlInfo, std::string> url_info);

  void OnInstallIsolatedWebApp(
      base::OnceCallback<void(MaybeInstallIsolatedWebAppCommandSuccess)>
          callback,
      base::expected<InstallIsolatedWebAppCommandSuccess,
                     InstallIsolatedWebAppCommandError> result);

  void ReportInstallationResult(
      MaybeInstallIsolatedWebAppCommandSuccess result);

  void MaybeScheduleGarbageCollection();

  raw_ref<Profile> profile_;

  raw_ptr<WebAppProvider> provider_ = nullptr;

  base::RepeatingCallback<void(
      base::expected<InstallIsolatedWebAppCommandSuccess, std::string>)>
      on_report_installation_result_ = base::DoNothing();

  // Signals when `GarbageCollectStoragePartitionsCommand` completes.
  base::OneShotEvent on_garbage_collect_storage_partitions_done_for_testing_;

  base::WeakPtrFactory<IsolatedWebAppInstallationManager> weak_ptr_factory_{
      this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLATION_MANAGER_H_
