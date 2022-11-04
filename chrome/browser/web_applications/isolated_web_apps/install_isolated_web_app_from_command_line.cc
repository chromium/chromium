// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_from_command_line.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/overloaded.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/types/expected.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolation_data.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_url_loader.h"
#include "chrome/common/chrome_switches.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "content/public/browser/web_contents.h"
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

std::unique_ptr<content::WebContents> CreateWebContents(Profile& profile) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(content::WebContents::CreateParams(
          /*context=*/&profile));

  webapps::InstallableManager::CreateForWebContents(web_contents.get());

  return web_contents;
}

void ScheduleInstallIsolatedWebApp(const IsolatedWebAppUrlInfo& isolation_info,
                                   IsolationData isolation_data,
                                   WebAppProvider& provider,
                                   Profile& profile) {
  provider.command_manager().ScheduleCommand(
      std::make_unique<InstallIsolatedWebAppCommand>(
          isolation_info, isolation_data, CreateWebContents(profile),
          std::make_unique<WebAppUrlLoader>(), profile,
          provider.install_finalizer(),
          base::BindOnce(&ReportInstallationResult)));
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
                      absolute_path.AsUTF8Unsafe(), "'"}));
  }

  return IsolationData{IsolationData::DevModeBundle{.path = absolute_path}};
}

}  // namespace

base::expected<IsolatedWebAppUrlInfo, std::string> GetIsolationInfo(
    const IsolationData& isolation_data) {
  return absl::visit(
      base::Overloaded{
          [](const IsolationData::InstalledBundle&)
              -> base::expected<IsolatedWebAppUrlInfo, std::string> {
            return base::unexpected(
                "Getting IsolationInfo from |InstalledBundle| is not "
                "implemented");
          },
          [](const IsolationData::DevModeBundle&)
              -> base::expected<IsolatedWebAppUrlInfo, std::string> {
            return base::unexpected(
                "Getting IsolationInfo from |DevModeBundle| is not "
                "implemented");
          },
          [](const IsolationData::DevModeProxy&)
              -> base::expected<IsolatedWebAppUrlInfo, std::string> {
            return IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
                web_package::SignedWebBundleId::CreateRandomForDevelopment());
          }},
      isolation_data.content);
}

base::expected<absl::optional<IsolationData>, std::string>
GetIsolationDataFromCommandLine(const base::CommandLine& command_line) {
  base::expected<absl::optional<IsolationData>, std::string> proxy_url =
      GetProxyUrlFromCommandLine(command_line);
  base::expected<absl::optional<IsolationData>, std::string> bundle_path =
      GetBundlePathFromCommandLine(command_line);

  // Return an error if both flags are set.
  bool was_proxy_url_set = !proxy_url.has_value() || proxy_url->has_value();
  bool was_bundle_path_set =
      !bundle_path.has_value() || bundle_path->has_value();
  if (was_proxy_url_set && was_bundle_path_set) {
    return base::unexpected(
        base::StrCat({"--", switches::kInstallIsolatedWebAppFromUrl, " and --",
                      switches::kInstallIsolatedWebAppFromFile,
                      " cannot both be provided."}));
  }

  return was_proxy_url_set ? proxy_url : bundle_path;
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
      GetIsolationDataFromCommandLine(command_line);
  if (!isolation_data.has_value()) {
    LOG(ERROR) << isolation_data.error();
    return;
  }
  if (!isolation_data->has_value()) {
    return;
  }

  base::expected<IsolatedWebAppUrlInfo, std::string> isolation_info =
      GetIsolationInfo(**isolation_data);

  if (!isolation_info.has_value()) {
    LOG(ERROR) << isolation_info.error();
    return;
  }

  provider->on_registry_ready().Post(
      FROM_HERE,
      base::BindOnce(&ScheduleInstallIsolatedWebApp, isolation_info.value(),
                     **isolation_data, std::ref(*provider), std::ref(profile)));
}

}  // namespace web_app
