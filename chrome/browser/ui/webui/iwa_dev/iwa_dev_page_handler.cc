// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/iwa_dev/iwa_dev_page_handler.h"

#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest_fetcher.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/model/isolation_data.h"
#include "chrome/browser/web_applications/model/iwa_update_info.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/isolated_web_apps/types/iwa_origin.h"
#include "components/webapps/isolated_web_apps/types/source.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"

namespace {

constexpr auto kUpdateManifestFetchAnnotation =
    net::DefinePartialNetworkTrafficAnnotation("iwa_dev_ui_update_manifest",
                                               "iwa_update_manifest_fetcher",
                                               R"(
    semantics {
      sender: "IWA Dev UI Page"
      description:
        "Downloads the Update Manifest of an Isolated Web App. "
        "The Update Manifest contains the list of the available versions of "
        "the IWA and the URL to the Signed Web Bundles that correspond to each "
        "version."
      trigger:
        "User inputs an Update Manifest URL in chrome://iwa-dev."
      destination_other: "The URL specified by the user in chrome://iwa-dev"
    }
    policy {
      setting: "This feature cannot be disabled in settings."
      policy_exception_justification: "Not implemented."
    })");

iwa_dev::mojom::IwaDevModeSourcePtr MapToMojomIwaDevModeSource(
    const web_app::IsolationData& isolation_data) {
  if (isolation_data.update_manifest_url()) {
    return iwa_dev::mojom::IwaDevModeSource::NewUpdateInfo(
        iwa_dev::mojom::UpdateInfo::New(
            *isolation_data.update_manifest_url(),
            isolation_data.update_channel()
                .value_or(web_app::UpdateChannel::default_channel())
                .ToString()));
  }

  return std::visit(
      absl::Overload{
          [](const web_app::IwaStorageOwnedBundle& location)
              -> iwa_dev::mojom::IwaDevModeSourcePtr {
            return iwa_dev::mojom::IwaDevModeSource::NewBundlePath(
                base::FilePath(FILE_PATH_LITERAL("..."))
                    .AppendASCII(location.dir_name_ascii())
                    .Append(web_app::kMainSwbnFileName));
          },
          [](const web_app::IwaStorageUnownedBundle& location)
              -> iwa_dev::mojom::IwaDevModeSourcePtr {
            return iwa_dev::mojom::IwaDevModeSource::NewBundlePath(
                base::FilePath(FILE_PATH_LITERAL("..."))
                    .Append(location.path().DirName().BaseName())
                    .Append(location.path().BaseName()));
          },
          [](const web_app::IwaStorageProxy& location)
              -> iwa_dev::mojom::IwaDevModeSourcePtr {
            return iwa_dev::mojom::IwaDevModeSource::NewProxyOrigin(
                location.proxy_url());
          },
      },
      isolation_data.location().variant());
}

iwa_dev::mojom::IwaDevModeAppInfoPtr MapToMojomIwaDevModeAppInfo(
    const web_app::WebApp& app) {
  auto iwa_origin = web_app::IwaOrigin::Create(app.start_url()).value();
  std::string web_bundle_id = iwa_origin.web_bundle_id().id();

  const web_app::IsolationData& isolation_data = app.isolation_data().value();

  return iwa_dev::mojom::IwaDevModeAppInfo::New(
      app.app_id(), std::move(web_bundle_id), app.untranslated_name(),
      MapToMojomIwaDevModeSource(isolation_data),
      isolation_data.version().GetString());
}

std::optional<std::string> MapToInstallError(
    web_app::IsolatedWebAppDevInstallManager::
        MaybeInstallIsolatedWebAppCommandSuccess result) {
  return result.has_value() ? std::nullopt : std::make_optional(result.error());
}

iwa_dev::mojom::UpdateManifestPtr MapToMojomUpdateManifest(
    const web_app::UpdateManifest& update_manifest) {
  auto update_manifest_ptr = iwa_dev::mojom::UpdateManifest::New();

  base::flat_set<web_app::UpdateChannel> channels;
  update_manifest_ptr->versions.reserve(update_manifest.versions().size());

  for (const auto& ve : update_manifest.versions()) {
    channels.insert_range(ve.channels());
    update_manifest_ptr->versions.push_back(iwa_dev::mojom::VersionEntry::New(
        ve.version().GetString(), ve.src(),
        base::ToVector(ve.channels(), &web_app::UpdateChannel::ToString)));
  }

  update_manifest_ptr->channels =
      base::ToVector(channels, [&](const web_app::UpdateChannel& channel) {
        return iwa_dev::mojom::ChannelMetadata::New(
            channel.ToString(),
            update_manifest.GetChannelMetadata(channel).GetDisplayName());
      });

  return update_manifest_ptr;
}

}  // namespace

class IwaDevPageHandler::LocalBundleSelectListener
    : public content::FileSelectListener {
 public:
  explicit LocalBundleSelectListener(
      base::OnceCallback<void(std::optional<base::FilePath>)> callback)
      : callback_(std::move(callback)) {}

  void Show(content::RenderFrameHost* render_frame_host) {
    blink::mojom::FileChooserParams params;
    params.mode = blink::mojom::FileChooserParams::Mode::kOpen;
    params.need_local_path = true;
    params.accept_types.push_back(u".swbn");

    FileSelectHelper::RunFileChooser(render_frame_host,
                                     base::WrapRefCounted(this), params);
  }

  // content::FileSelectListener
  void FileSelected(std::vector<blink::mojom::FileChooserFileInfoPtr> files,
                    const base::FilePath& base_dir,
                    blink::mojom::FileChooserParams::Mode mode) override {
    CHECK(callback_);
    // `params.mode` is kOpen so a single file should have been selected.
    CHECK_EQ(files.size(), 1u);
    auto& file = *files[0];
    // `params.need_local_path` is true so the result should be a native file.
    CHECK(file.is_native_file());
    std::move(callback_).Run(file.get_native_file()->file_path);
  }

  void FileSelectionCanceled() override {
    CHECK(callback_);
    std::move(callback_).Run(std::nullopt);
  }

 private:
  ~LocalBundleSelectListener() override = default;

  base::OnceCallback<void(std::optional<base::FilePath>)> callback_;
};

IwaDevPageHandler::IwaDevPageHandler(
    content::WebUI* web_ui,
    mojo::PendingRemote<iwa_dev::mojom::Page> page,
    mojo::PendingReceiver<iwa_dev::mojom::PageHandler> receiver)
    : profile_(CHECK_DEREF(Profile::FromWebUI(web_ui))),
      provider_(
          CHECK_DEREF(web_app::WebAppProvider::GetForWebApps(&profile_.get()))),
      web_contents_(CHECK_DEREF(web_ui->GetWebContents())),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
  install_observation_.Observe(&provider_->install_manager());
}

IwaDevPageHandler::~IwaDevPageHandler() = default;

void IwaDevPageHandler::GetInstalledAppsInfo(
    GetInstalledAppsInfoCallback callback) {
  std::vector<iwa_dev::mojom::IwaDevModeAppInfoPtr> dev_mode_apps;
  for (const web_app::WebApp& app : provider_->registrar_unsafe().GetApps(
           web_app::WebAppFilter::IsDevModeIsolatedApp())) {
    dev_mode_apps.push_back(MapToMojomIwaDevModeAppInfo(app));
  }

  std::move(callback).Run(std::move(dev_mode_apps));
}

void IwaDevPageHandler::UninstallApp(const std::string& app_id,
                                     UninstallAppCallback callback) {
  // If the app is no longer installed (e.g. uninstalled via another window),
  // or the webui compromised, this should be ignored.
  if (!provider_->registrar_unsafe().GetAppById(
          app_id, web_app::WebAppFilter::IsDevModeIsolatedApp())) {
    std::move(callback).Run(false);
    return;
  }

  provider_->ui_manager().PresentUserUninstallDialog(
      app_id, webapps::WebappUninstallSource::kAppsPage,
      web_contents_->GetTopLevelNativeWindow(),
      base::BindOnce(&webapps::UninstallSucceeded).Then(std::move(callback)));
}

void IwaDevPageHandler::InstallAppFromDevProxy(
    const GURL& url,
    InstallAppFromDevProxyCallback callback) {
  provider_->isolated_web_app_dev_install_manager()
      .InstallIsolatedWebAppFromDevModeProxy(
          url, web_app::IsolatedWebAppDevInstallManager::InstallSurface::kDevUi,
          base::BindOnce(&MapToInstallError).Then(std::move(callback)));
}

void IwaDevPageHandler::InstallAppFromUpdateManifest(
    const GURL& web_bundle_url,
    iwa_dev::mojom::UpdateInfoPtr update_info,
    InstallAppFromUpdateManifestCallback callback) {
  if (!web_bundle_url.SchemeIsHTTPOrHTTPS()) {
    std::move(callback).Run("Invalid Web Bundle URL provided.");
    return;
  }
  if (!update_info->update_manifest_url.SchemeIsHTTPOrHTTPS()) {
    std::move(callback).Run("Invalid Update Manifest URL provided.");
    return;
  }
  ASSIGN_OR_RETURN(
      web_app::UpdateChannel update_channel,
      web_app::UpdateChannel::Create(update_info->update_channel), [&](auto) {
        std::move(callback).Run("Invalid update channel provided.");
      });

  provider_->isolated_web_app_dev_install_manager()
      .DownloadAndInstallIsolatedWebAppFromDevModeBundle(
          web_bundle_url,
          web_app::IsolatedWebAppDevInstallManager::InstallSurface::kDevUi,
          base::BindOnce(&MapToInstallError).Then(std::move(callback)),
          /*expected_bundle_id=*/std::nullopt,
          web_app::IwaUpdateInfo(update_info->update_manifest_url,
                                 std::move(update_channel)));
}

void IwaDevPageHandler::ParseUpdateManifestFromUrl(
    const GURL& update_manifest_url,
    ParseUpdateManifestFromUrlCallback callback) {
  auto fetcher = std::make_unique<web_app::UpdateManifestFetcher>(
      update_manifest_url, kUpdateManifestFetchAnnotation,
      profile_->GetURLLoaderFactory(),
      profile_->GetDefaultStoragePartition()->GetNetworkContext());

  auto* fetcher_ptr = fetcher.get();

  base::OnceClosure fetcher_keep_alive =
      base::DoNothingWithBoundArgs(std::move(fetcher));
  fetcher_ptr->FetchUpdateManifest(
      base::BindOnce(
          [](base::expected<web_app::UpdateManifest,
                            web_app::UpdateManifestFetcher::Error> result) {
            return std::move(result)
                .transform(&MapToMojomUpdateManifest)
                .transform_error(
                    [](web_app::UpdateManifestFetcher::Error error) {
                      return base::StrCat(
                          {"Manifest fetch failed: ",
                           web_app::UpdateManifestFetcher::ErrorToString(
                               error)});
                    });
          })
          .Then(std::move(callback))
          .Then(std::move(fetcher_keep_alive)));
}

void IwaDevPageHandler::SelectAndInstallAppFromLocalWebBundle(
    SelectAndInstallAppFromLocalWebBundleCallback callback) {
  content::RenderFrameHost* render_frame_host =
      web_contents_->GetPrimaryMainFrame();

  base::MakeRefCounted<LocalBundleSelectListener>(
      base::BindOnce(&IwaDevPageHandler::OnLocalBundleSelected,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)))
      ->Show(render_frame_host);
}

void IwaDevPageHandler::OnLocalBundleSelected(
    SelectAndInstallAppFromLocalWebBundleCallback callback,
    std::optional<base::FilePath> path) {
  if (!path) {
    std::move(callback).Run("No file selected");
    return;
  }

  provider_->isolated_web_app_dev_install_manager()
      .InstallIsolatedWebAppFromDevModeBundle(
          *path,
          web_app::IsolatedWebAppDevInstallManager::InstallSurface::kDevUi,
          base::BindOnce(&MapToInstallError).Then(std::move(callback)));
}

void IwaDevPageHandler::OnWebAppInstalled(const webapps::AppId& app_id) {
  if (const web_app::WebApp* app = provider_->registrar_unsafe().GetAppById(
          app_id, web_app::WebAppFilter::IsDevModeIsolatedApp())) {
    page_->OnAppInstalled(MapToMojomIwaDevModeAppInfo(*app));
  }
}

void IwaDevPageHandler::OnWebAppManifestUpdated(const webapps::AppId& app_id) {
  if (const web_app::WebApp* app = provider_->registrar_unsafe().GetAppById(
          app_id, web_app::WebAppFilter::IsDevModeIsolatedApp())) {
    page_->OnAppUpdated(MapToMojomIwaDevModeAppInfo(*app));
  }
}

void IwaDevPageHandler::OnWebAppWillBeUninstalled(
    const webapps::AppId& app_id) {
  if (provider_->registrar_unsafe().AppMatches(
          app_id, web_app::WebAppFilter::IsDevModeIsolatedApp())) {
    page_->OnAppUninstalled(app_id);
  }
}

void IwaDevPageHandler::OnWebAppInstallManagerDestroyed() {
  install_observation_.Reset();
}
