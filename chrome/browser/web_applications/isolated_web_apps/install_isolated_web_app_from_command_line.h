// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_INSTALL_ISOLATED_WEB_APP_FROM_COMMAND_LINE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_INSTALL_ISOLATED_WEB_APP_FROM_COMMAND_LINE_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class CommandLine;
enum class TaskPriority : uint8_t;
}

class Profile;

namespace web_app {

class IsolatedWebAppUrlInfo;
class WebAppProvider;

void GetIsolatedWebAppLocationFromCommandLine(
    const base::CommandLine& command_line,
    base::OnceCallback<void(
        base::expected<absl::optional<IsolatedWebAppLocation>, std::string>)>
        callback);

bool HasIwaInstallSwitch(const base::CommandLine& command_line);

// This class manages installation of Isolated Web Apps triggered by command
// line switches (`switches::kInstallIsolatedWebAppFromUrl` and
// `switches::kInstallIsolatedWebAppFromFile`).
//
// The `InstallFromCommandLine` method can be used to imperatively parse the
// provided command line and install an IWA if specified.
//
// On ChromeOS only, the command line will be parsed whenever a new manager is
// started, which occurs on `Profile` initialization. This is done this way
// because the browser does not go through the "normal" startup flow on
// ChromeOS, and has different startup behaviors depending on whether or not Ash
// or Lacros is used.
//
// TODO(cmfcmf): Revisit this behavior once using Ash instead of Lacros is no
// longer possible.
class IsolatedWebAppCommandLineInstallManager {
 public:
  explicit IsolatedWebAppCommandLineInstallManager(Profile& profile);
  ~IsolatedWebAppCommandLineInstallManager();

  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);

  void Start();

  // Install an IWA from command line, if the command line specifies the
  // appropriate switches.
  void InstallFromCommandLine(
      const base::CommandLine& command_line,
      std::unique_ptr<ScopedKeepAlive> keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
      base::TaskPriority task_priority);

  void OnReportInstallationResultForTesting(
      base::RepeatingCallback<void(
          base::expected<InstallIsolatedWebAppCommandSuccess, std::string>)>
          on_report_installation_result) {
    on_report_installation_result_ = std::move(on_report_installation_result);
  }

 private:
  void OnGetIsolatedWebAppLocationFromCommandLine(
      std::unique_ptr<ScopedKeepAlive> keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
      base::expected<absl::optional<IsolatedWebAppLocation>, std::string>
          location);

  void OnGetIsolatedWebAppUrlInfo(
      std::unique_ptr<ScopedKeepAlive> keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
      const IsolatedWebAppLocation& location,
      base::expected<IsolatedWebAppUrlInfo, std::string> url_info);

  void OnInstallIsolatedWebApp(
      base::expected<InstallIsolatedWebAppCommandSuccess,
                     InstallIsolatedWebAppCommandError> result);

  void ReportInstallationResult(
      base::expected<InstallIsolatedWebAppCommandSuccess, std::string> result);

  raw_ref<Profile> profile_;

  raw_ptr<WebAppProvider> provider_ = nullptr;

  base::RepeatingCallback<void(
      base::expected<InstallIsolatedWebAppCommandSuccess, std::string>)>
      on_report_installation_result_ = base::DoNothing();

  base::WeakPtrFactory<IsolatedWebAppCommandLineInstallManager>
      weak_ptr_factory_{this};
};

// Attempts to install an IWA if the respective command line parameters are
// provided. It might silently fail for multiple reasons, such as:
// - missing command line parameters
// - missing `WebAppProvider`
// - browser shutting down
void MaybeInstallIwaFromCommandLine(const base::CommandLine& command_line_,
                                    Profile& profile);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_INSTALL_ISOLATED_WEB_APP_FROM_COMMAND_LINE_H_
