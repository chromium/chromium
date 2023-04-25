// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_from_command_line.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/types/expected.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_dev_mode.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_switches.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

namespace {

void ReportInstallationResult(
    base::expected<InstallIsolatedWebAppCommandSuccess,
                   InstallIsolatedWebAppCommandError> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "Isolated web app auto installation failed. Error: "
               << result.error();
  }
}

base::expected<absl::optional<IsolatedWebAppLocation>, std::string>
GetProxyUrlFromCommandLine(const base::CommandLine& command_line) {
  std::string switch_value =
      command_line.GetSwitchValueASCII(switches::kInstallIsolatedWebAppFromUrl);

  if (switch_value.empty()) {
    return absl::nullopt;
  }

  GURL url{switch_value};
  url::Origin url_origin = url::Origin::Create(url);

  if (!url.is_valid() || url_origin.opaque()) {
    return base::unexpected(base::StrCat(
        {"Invalid URL provided to --", switches::kInstallIsolatedWebAppFromUrl,
         " flag: '", url.possibly_invalid_spec(), "'"}));
  }

  if (url_origin.GetURL() != url) {
    return base::unexpected(base::StrCat(
        {"Non-origin URL provided to --",
         switches::kInstallIsolatedWebAppFromUrl, " flag: '",
         url.possibly_invalid_spec(), "'", ". Possible origin URL: '",
         url_origin.Serialize(), "'."}));
  }

  return DevModeProxy{.proxy_url = url_origin};
}

base::expected<absl::optional<IsolatedWebAppLocation>, std::string>
GetBundlePathFromCommandLine(const base::CommandLine& command_line) {
  base::FilePath switch_value =
      command_line.GetSwitchValuePath(switches::kInstallIsolatedWebAppFromFile);

  if (switch_value.empty()) {
    return absl::nullopt;
  }

  base::FilePath absolute_path = base::MakeAbsoluteFilePath(switch_value);

  if (!base::PathExists(absolute_path) ||
      base::DirectoryExists(absolute_path)) {
    return base::unexpected(
        base::StrCat({"Invalid path provided to --",
                      switches::kInstallIsolatedWebAppFromFile, " flag: '",
                      switch_value.AsUTF8Unsafe(), "'"}));
  }

  return DevModeBundle{.path = absolute_path};
}

}  // namespace

void OnGetIsolatedWebAppUrlInfo(
    WebAppProvider* provider,
    const IsolatedWebAppLocation& location,
    base::expected<IsolatedWebAppUrlInfo, std::string> url_info) {
  if (!url_info.has_value()) {
    LOG(ERROR) << "Failed to get IsolationInfo: " << url_info.error();
    return;
  }

  // TODO(cmfcmf): Keep alives need to be set here.
  provider->scheduler().InstallIsolatedWebApp(
      url_info.value(), location, /*keep_alive=*/nullptr,
      /*profile_keep_alive=*/nullptr,
      base::BindOnce(&ReportInstallationResult));
}

base::expected<absl::optional<IsolatedWebAppLocation>, std::string>
GetIsolatedWebAppLocationFromCommandLine(const base::CommandLine& command_line,
                                         const PrefService* prefs) {
  base::expected<absl::optional<IsolatedWebAppLocation>, std::string>
      proxy_url = GetProxyUrlFromCommandLine(command_line);
  base::expected<absl::optional<IsolatedWebAppLocation>, std::string>
      bundle_path = GetBundlePathFromCommandLine(command_line);

  bool is_proxy_url_set = !proxy_url.has_value() || proxy_url->has_value();
  bool is_bundle_path_set =
      !bundle_path.has_value() || bundle_path->has_value();
  if (!is_proxy_url_set && !is_bundle_path_set) {
    return absl::nullopt;
  }

  if (!prefs || !IsIwaDevModeEnabled(*prefs)) {
    return base::unexpected<std::string>(kIwaDevModeNotEnabledMessage);
  }

  if (is_proxy_url_set && is_bundle_path_set) {
    return base::unexpected(
        base::StrCat({"--", switches::kInstallIsolatedWebAppFromUrl, " and --",
                      switches::kInstallIsolatedWebAppFromFile,
                      " cannot both be provided."}));
  }

  return is_proxy_url_set ? proxy_url : bundle_path;
}

void MaybeInstallAppFromCommandLine(const base::CommandLine& command_line,
                                    Profile& profile) {
  // Web applications are not available on some platforms and
  // |WebAppProvider::GetForWebApps| returns nullptr in such cases.
  //
  // See |WebAppProvider::GetForWebApps| documentation for details.
  WebAppProvider* provider = WebAppProvider::GetForWebApps(&profile);
  if (provider == nullptr) {
    return;
  }

  base::expected<absl::optional<IsolatedWebAppLocation>, std::string> location =
      GetIsolatedWebAppLocationFromCommandLine(command_line,
                                               profile.GetPrefs());
  // Check the base::expected.
  if (!location.has_value()) {
    LOG(ERROR) << location.error();
    return;
  }
  // Check the absl::optional.
  if (!location->has_value()) {
    return;
  }

  IsolatedWebAppUrlInfo::CreateFromIsolatedWebAppLocation(
      **location,
      base::BindOnce(&OnGetIsolatedWebAppUrlInfo, provider, **location));
}

}  // namespace web_app
