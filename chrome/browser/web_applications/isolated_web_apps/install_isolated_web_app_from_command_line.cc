// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_from_command_line.h"

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/types/expected.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolation_data.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "content/public/common/content_features.h"
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
    LOG(ERROR) << "Isolated web app auto installation "
                  "failed. Error: "
               << result.error();
  }
}

base::expected<absl::optional<IsolationData>, std::string>
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

  return IsolationData{IsolationData::DevModeProxy{.proxy_url = url_origin}};
}

base::expected<absl::optional<IsolationData>, std::string>
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

  return IsolationData{IsolationData::DevModeBundle{.path = absolute_path}};
}

}  // namespace

void OnGetIsolatedWebAppUrlInfo(
    WebAppProvider* provider,
    const IsolationData& isolation_data,
    base::expected<IsolatedWebAppUrlInfo, std::string> isolation_info) {
  if (!isolation_info.has_value()) {
    LOG(ERROR) << base::StrCat(
        {"Failed to get IsolationInfo: ", isolation_info.error()});
    return;
  }

  provider->scheduler().InstallIsolatedWebApp(
      isolation_info.value(), isolation_data,
      base::BindOnce(&ReportInstallationResult));
}

base::expected<absl::optional<IsolationData>, std::string>
GetIsolationDataFromCommandLine(const base::CommandLine& command_line,
                                const PrefService* prefs) {
  base::expected<absl::optional<IsolationData>, std::string> proxy_url =
      GetProxyUrlFromCommandLine(command_line);
  base::expected<absl::optional<IsolationData>, std::string> bundle_path =
      GetBundlePathFromCommandLine(command_line);

  bool is_proxy_url_set = !proxy_url.has_value() || proxy_url->has_value();
  bool is_bundle_path_set =
      !bundle_path.has_value() || bundle_path->has_value();
  if (!is_proxy_url_set && !is_bundle_path_set) {
    return absl::nullopt;
  }

  if (!base::FeatureList::IsEnabled(features::kIsolatedWebApps)) {
    return base::unexpected("Isolated Web Apps are not enabled");
  }

  if (is_proxy_url_set && is_bundle_path_set) {
    return base::unexpected(
        base::StrCat({"--", switches::kInstallIsolatedWebAppFromUrl, " and --",
                      switches::kInstallIsolatedWebAppFromFile,
                      " cannot both be provided."}));
  }

  bool is_dev_mode_policy_enabled =
      prefs && prefs->GetBoolean(
                   policy::policy_prefs::kIsolatedAppsDeveloperModeAllowed);
  if (!base::FeatureList::IsEnabled(features::kIsolatedWebAppDevMode) ||
      !is_dev_mode_policy_enabled) {
    return base::unexpected("Isolated Web App Developer Mode is not enabled");
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

  base::expected<absl::optional<IsolationData>, std::string> isolation_data =
      GetIsolationDataFromCommandLine(command_line, profile.GetPrefs());
  if (!isolation_data.has_value()) {
    LOG(ERROR) << isolation_data.error();
    return;
  }
  if (!isolation_data->has_value()) {
    return;
  }

  IsolatedWebAppUrlInfo::CreateFromIsolationData(
      **isolation_data,
      base::BindOnce(&OnGetIsolatedWebAppUrlInfo, provider, **isolation_data));
}

}  // namespace web_app
