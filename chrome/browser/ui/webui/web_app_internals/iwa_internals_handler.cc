// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/web_app_internals/iwa_internals_handler.h"

#include "base/containers/to_vector.h"
#include "base/functional/overloaded.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/ui/webui/web_app_internals/web_app_internals.mojom.h"
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_downloader.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_installation_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_discovery_task.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolation_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/iwa_key_distribution_info_provider.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest_fetcher.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_delegate.h"

namespace web_app {

namespace {

constexpr auto kUpdateManifestFetchAnnotation =
    net::DefinePartialNetworkTrafficAnnotation(
        "iwa_web_app_internals_update_manifest",
        "iwa_update_manifest_fetcher",
        R"(
    semantics {
      sender: "Web App Internals page"
      description:
        "Downloads the Update Manifest of an Isolated Web App. "
        "The Update Manifest contains the list of the available versions of "
        "the IWA and the URL to the Signed Web Bundles that correspond to each "
        "version."
      trigger:
        "User clicks on the discover button in chrome://web-app-internals."
    }
    policy {
      setting: "This feature cannot be disabled in settings."
      policy_exception_justification: "Not implemented."
    })");

constexpr auto kDownloadWebBundleAnnotation =
    net::DefinePartialNetworkTrafficAnnotation(
        "iwa_web_app_internals_web_bundle",
        "iwa_bundle_downloader",
        R"(
    semantics {
      sender: "Web App Internals page"
      description:
        "Downloads a Signed Web Bundle of an Isolated Web App which contains "
        "code and other resources of this app."
      trigger:
        "User accepts the installation dialog in chrome://web-app-internals."
    }
    policy {
      setting: "This feature cannot be disabled in settings."
      policy_exception_justification: "Not implemented."
    })");

void SendError(
    base::OnceCallback<void(::mojom::InstallIsolatedWebAppResultPtr)> callback,
    const std::string& error_message) {
  std::move(callback).Run(
      ::mojom::InstallIsolatedWebAppResult::NewError(error_message));
}

}  // namespace

class IwaInternalsHandler::IsolatedWebAppDevBundleSelectListener
    : public content::FileSelectListener {
 public:
  explicit IsolatedWebAppDevBundleSelectListener(
      base::OnceCallback<void(std::optional<base::FilePath>)> callback)
      : callback_(std::move(callback)) {}

  void Show(content::RenderFrameHost* render_frame_host) {
    blink::mojom::FileChooserParams params;
    params.mode = blink::mojom::FileChooserParams::Mode::kOpen;
    params.need_local_path = true;
    params.accept_types.push_back(u".swbn");

    FileSelectHelper::RunFileChooser(render_frame_host, this, params);
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
  ~IsolatedWebAppDevBundleSelectListener() override = default;

  base::OnceCallback<void(std::optional<base::FilePath>)> callback_;
};

class IwaInternalsHandler::IwaManifestInstallUpdateHandler
    : public IsolatedWebAppUpdateManager::Observer {
 public:
  explicit IwaManifestInstallUpdateHandler(WebAppProvider& provider)
      : provider_(provider) {
    observation_.Observe(&provider.iwa_update_manager());
  }

  void UpdateManifestInstalledIsolatedWebApp(
      const webapps::AppId& app_id,
      Handler::UpdateManifestInstalledIsolatedWebAppCallback callback) {
    if (base::Contains(update_requests_, app_id)) {
      std::move(callback).Run(
          "Update check skipped: please wait for the pending update request to "
          "resolve first.");
      return;
    }

    ASSIGN_OR_RETURN(
        const WebApp& iwa,
        GetIsolatedWebAppById(provider_->registrar_unsafe(), app_id),
        [&](const std::string& error) { std::move(callback).Run(error); });

    ASSIGN_OR_RETURN(
        const IsolatedWebAppUrlInfo& url_info,
        IsolatedWebAppUrlInfo::Create(iwa.manifest_id()),
        [&](const std::string& error) { std::move(callback).Run(error); })

    const IsolationData& isolation_data = *iwa.isolation_data();
    if (!isolation_data.location().dev_mode() ||
        !isolation_data.update_manifest_url()) {
      std::move(callback).Run(
          "Only dev-mode apps with update_manifest_url set can be updated via "
          "this routine.");
      return;
    }

    update_requests_.emplace(app_id, std::move(callback));
    // TODO(b/371521930): introduce channel switching for dev mode apps.
    provider_->iwa_update_manager().DiscoverUpdatesForApp(
        url_info, *isolation_data.update_manifest_url(),
        UpdateChannel::default_channel(), /*dev_mode=*/true);
  }

  // IsolatedWebAppUpdateManager::Observer:
  void OnUpdateDiscoveryTaskCompleted(
      const webapps::AppId& app_id,
      IsolatedWebAppUpdateDiscoveryTask::CompletionStatus status) override {
    if (status.has_value() && *status ==
                                  IsolatedWebAppUpdateDiscoveryTask::Success::
                                      kUpdateFoundAndSavedInDatabase) {
      return;
    }

    ASSIGN_OR_RETURN(auto callback, ConsumeUpdateRequest(app_id), [](auto) {});
    if (status.has_value()) {
      std::move(callback).Run(
          "Update skipped: app is already on the latest version.");
    } else {
      std::move(callback).Run(
          "Update failed: " +
          IsolatedWebAppUpdateDiscoveryTask::ErrorToString(status.error()));
    }
  }

  void OnUpdateApplyTaskCompleted(
      const webapps::AppId& app_id,
      IsolatedWebAppUpdateApplyTask::CompletionStatus status) override {
    ASSIGN_OR_RETURN(auto callback, ConsumeUpdateRequest(app_id), [](auto) {});

    ASSIGN_OR_RETURN(
        const WebApp& iwa,
        GetIsolatedWebAppById(provider_->registrar_unsafe(), app_id),
        [&](const std::string& error) { std::move(callback).Run(error); });

    std::move(callback).Run(
        status.has_value()
            ? base::StringPrintf("Update to v%s successful (refresh the page "
                                 "to reflect the update).",
                                 iwa.isolation_data()->version().GetString())
            : "Update failed: " + status.error().message);
  }

 private:
  // Retrieves the pending request for `app_id` and erases the entry from the
  // requests map. If there's no matching entry for `app_id`, returns an
  // unexpected.
  base::expected<Handler::UpdateDevProxyIsolatedWebAppCallback, absl::monostate>
  ConsumeUpdateRequest(const webapps::AppId& app_id) {
    auto itr = update_requests_.find(app_id);
    if (itr == update_requests_.end()) {
      return base::unexpected(absl::monostate());
    }

    auto callback = std::move(itr->second);
    update_requests_.erase(itr);

    return std::move(callback);
  }

  const raw_ref<WebAppProvider> provider_;

  base::ScopedObservation<IsolatedWebAppUpdateManager,
                          IwaManifestInstallUpdateHandler>
      observation_{this};

  // Multiple parallel requests for one `app_id` are not allowed.
  base::flat_map<webapps::AppId,
                 Handler::UpdateManifestInstalledIsolatedWebAppCallback>
      update_requests_;
};

IwaInternalsHandler::IwaInternalsHandler(content::WebUI& web_ui,
                                         Profile& profile)
    : web_ui_(web_ui), profile_(profile) {
  if (auto* provider = WebAppProvider::GetForWebApps(&profile)) {
    update_handler_ =
        std::make_unique<IwaManifestInstallUpdateHandler>(*provider);
  }
}

IwaInternalsHandler::~IwaInternalsHandler() = default;

void IwaInternalsHandler::InstallIsolatedWebAppFromDevProxy(
    const GURL& url,
    Handler::InstallIsolatedWebAppFromDevProxyCallback callback) {
  auto* provider = WebAppProvider::GetForWebApps(profile());
  if (!provider) {
    SendError(std::move(callback), "could not get web app provider");
    return;
  }

  auto& manager = provider->isolated_web_app_installation_manager();
  manager.InstallIsolatedWebAppFromDevModeProxy(
      url, IsolatedWebAppInstallationManager::InstallSurface::kDevUi,
      base::BindOnce(&IwaInternalsHandler::OnInstallIsolatedWebAppInDevMode,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void IwaInternalsHandler::ParseUpdateManifestFromUrl(
    const GURL& update_manifest_url,
    Handler::ParseUpdateManifestFromUrlCallback callback) {
  if (!WebAppProvider::GetForWebApps(profile())) {
    std::move(callback).Run(::mojom::ParseUpdateManifestFromUrlResult::NewError(
        "Couldn't get the WebAppProvider."));
    return;
  }

  auto fetcher = std::make_unique<UpdateManifestFetcher>(
      update_manifest_url, kUpdateManifestFetchAnnotation,
      profile()->GetURLLoaderFactory());
  auto* fetcher_ptr = fetcher.get();

  base::OnceClosure fetcher_keep_alive =
      base::DoNothingWithBoundArgs(std::move(fetcher));
  fetcher_ptr->FetchUpdateManifest(
      base::BindOnce(
          [](const GURL& update_manifest_url,
             base::expected<UpdateManifest, UpdateManifestFetcher::Error>
                 result) -> ::mojom::ParseUpdateManifestFromUrlResultPtr {
            ASSIGN_OR_RETURN(
                auto update_manifest, std::move(result), [](auto error) {
                  return ::mojom::ParseUpdateManifestFromUrlResult::NewError(
                      "Manifest fetch failed.");
                });
            auto update_manifest_ptr = ::mojom::UpdateManifest::New();
            update_manifest_ptr->versions = base::ToVector(
                update_manifest.versions(),
                [](const UpdateManifest::VersionEntry& ve) {
                  auto version_entry = ::mojom::VersionEntry::New();
                  version_entry->version = ve.version().GetString();
                  version_entry->web_bundle_url = ve.src();
                  return version_entry;
                });
            return ::mojom::ParseUpdateManifestFromUrlResult::NewUpdateManifest(
                std::move(update_manifest_ptr));
          },
          update_manifest_url)
          .Then(std::move(callback))
          .Then(std::move(fetcher_keep_alive)));
}

void IwaInternalsHandler::InstallIsolatedWebAppFromBundleUrl(
    ::mojom::InstallFromBundleUrlParamsPtr params,
    Handler::InstallIsolatedWebAppFromBundleUrlCallback callback) {
  if (!WebAppProvider::GetForWebApps(profile())) {
    std::move(callback).Run(::mojom::InstallIsolatedWebAppResult::NewError(
        "WebAppProvider not supported for current profile."));
    return;
  }
  ScopedTempWebBundleFile::Create(
      base::BindOnce(&IwaInternalsHandler::DownloadWebBundleToFile,
                     weak_ptr_factory_.GetWeakPtr(), params->web_bundle_url,
                     params->update_manifest_url, std::move(callback)));
}

void IwaInternalsHandler::SelectFileAndInstallIsolatedWebAppFromDevBundle(
    Handler::SelectFileAndInstallIsolatedWebAppFromDevBundleCallback callback) {
  content::RenderFrameHost* render_frame_host = web_ui_->GetRenderFrameHost();
  if (!render_frame_host) {
    SendError(std::move(callback), "could not get render frame host");
    return;
  }

  base::MakeRefCounted<IsolatedWebAppDevBundleSelectListener>(
      base::BindOnce(
          &IwaInternalsHandler::OnIsolatedWebAppDevModeBundleSelected,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)))
      ->Show(render_frame_host);
}

void IwaInternalsHandler::OnIsolatedWebAppDevModeBundleSelected(
    Handler::SelectFileAndInstallIsolatedWebAppFromDevBundleCallback callback,
    std::optional<base::FilePath> path) {
  if (!path) {
    SendError(std::move(callback), "no file selected");
    return;
  }

  auto* provider = WebAppProvider::GetForWebApps(profile());
  if (!provider) {
    SendError(std::move(callback), "could not get web app provider");
    return;
  }

  auto& manager = provider->isolated_web_app_installation_manager();
  manager.InstallIsolatedWebAppFromDevModeBundle(
      *path, IsolatedWebAppInstallationManager::InstallSurface::kDevUi,
      base::BindOnce(&IwaInternalsHandler::OnInstallIsolatedWebAppInDevMode,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void IwaInternalsHandler::SelectFileAndUpdateIsolatedWebAppFromDevBundle(
    const webapps::AppId& app_id,
    Handler::SelectFileAndUpdateIsolatedWebAppFromDevBundleCallback callback) {
  content::RenderFrameHost* render_frame_host = web_ui_->GetRenderFrameHost();
  if (!render_frame_host) {
    std::move(callback).Run("could not get render frame host");
    return;
  }

  base::MakeRefCounted<IsolatedWebAppDevBundleSelectListener>(
      base::BindOnce(
          &IwaInternalsHandler::OnIsolatedWebAppDevModeBundleSelectedForUpdate,
          weak_ptr_factory_.GetWeakPtr(), app_id, std::move(callback)))
      ->Show(render_frame_host);
}

void IwaInternalsHandler::OnIsolatedWebAppDevModeBundleSelectedForUpdate(
    const webapps::AppId& app_id,
    Handler::SelectFileAndUpdateIsolatedWebAppFromDevBundleCallback callback,
    std::optional<base::FilePath> path) {
  if (!path) {
    std::move(callback).Run("no file selected");
    return;
  }

  IwaSourceDevModeWithFileOp source(
      IwaSourceBundleDevModeWithFileOp(*path, kDefaultBundleDevFileOp));
  ApplyDevModeUpdate(app_id, source, std::move(callback));
}

void IwaInternalsHandler::OnInstallIsolatedWebAppInDevMode(
    base::OnceCallback<void(::mojom::InstallIsolatedWebAppResultPtr)> callback,
    IsolatedWebAppInstallationManager::MaybeInstallIsolatedWebAppCommandSuccess
        result) {
  std::move(callback).Run([&] {
    if (result.has_value()) {
      auto success = ::mojom::InstallIsolatedWebAppSuccess::New();
      success->web_bundle_id = result->url_info.web_bundle_id().id();
      return ::mojom::InstallIsolatedWebAppResult::NewSuccess(
          std::move(success));
    }
    return ::mojom::InstallIsolatedWebAppResult::NewError(result.error());
  }());
}

void IwaInternalsHandler::SearchForIsolatedWebAppUpdates(
    Handler::SearchForIsolatedWebAppUpdatesCallback callback) {
  auto* provider = WebAppProvider::GetForWebApps(profile());
  if (!provider) {
    std::move(callback).Run("could not get web app provider");
    return;
  }

  size_t queued_task_count =
      provider->iwa_update_manager().DiscoverUpdatesNow();
  std::move(callback).Run(base::StringPrintf(
      "queued %zu update discovery tasks", queued_task_count));
}

void IwaInternalsHandler::GetIsolatedWebAppDevModeAppInfo(
    Handler::GetIsolatedWebAppDevModeAppInfoCallback callback) {
  if (!IsIwaDevModeEnabled(profile())) {
    std::move(callback).Run({});
    return;
  }

  auto* provider = WebAppProvider::GetForWebApps(profile());
  if (!provider) {
    std::move(callback).Run({});
    return;
  }

  std::vector<::mojom::IwaDevModeAppInfoPtr> dev_mode_apps;
  for (const WebApp& app : provider->registrar_unsafe().GetApps()) {
    if (!app.isolation_data().has_value()) {
      continue;
    }

    base::expected<IwaSourceDevMode, absl::monostate> source =
        IwaSourceDevMode::FromStorageLocation(profile()->GetPath(),
                                              app.isolation_data()->location());
    if (!source.has_value()) {
      continue;
    }

    const web_app::IsolationData& isolation_data = *app.isolation_data();
    absl::visit(
        base::Overloaded{
            [&](const IwaSourceBundleDevMode& source) {
              dev_mode_apps.emplace_back(::mojom::IwaDevModeAppInfo::New(
                  app.app_id(), app.untranslated_name(),
                  isolation_data.update_manifest_url()
                      ? ::mojom::IwaDevModeLocation::NewUpdateManifestUrl(
                            *isolation_data.update_manifest_url())
                      : ::mojom::IwaDevModeLocation::NewBundlePath(
                            source.path()),
                  app.isolation_data()->version().GetString()));
            },
            [&](const IwaSourceProxy& source) {
              dev_mode_apps.emplace_back(::mojom::IwaDevModeAppInfo::New(
                  app.app_id(), app.untranslated_name(),
                  ::mojom::IwaDevModeLocation::NewProxyOrigin(
                      source.proxy_url()),
                  app.isolation_data()->version().GetString()));
            },
        },
        source->variant());
  }

  std::move(callback).Run(std::move(dev_mode_apps));
}

void IwaInternalsHandler::UpdateDevProxyIsolatedWebApp(
    const webapps::AppId& app_id,
    Handler::UpdateDevProxyIsolatedWebAppCallback callback) {
  ApplyDevModeUpdate(app_id,
                     // For dev mode proxy apps, the location remains the same
                     // and does not change between updates.
                     /*location=*/std::nullopt, std::move(callback));
}

void IwaInternalsHandler::ApplyDevModeUpdate(
    const webapps::AppId& app_id,
    base::optional_ref<const IwaSourceDevModeWithFileOp> location,
    base::OnceCallback<void(const std::string&)> callback) {
  if (!IsIwaDevModeEnabled(profile())) {
    std::move(callback).Run("IWA dev mode is not enabled");
    return;
  }

  auto* provider = WebAppProvider::GetForWebApps(profile());
  if (!provider) {
    std::move(callback).Run("could not get web app provider");
    return;
  }

  auto* app = provider->registrar_unsafe().GetAppById(app_id);
  if (!app || !app->isolation_data().has_value()) {
    std::move(callback).Run("could not find installed IWA");
    return;
  }
  ASSIGN_OR_RETURN(IwaSourceDevMode source,
                   IwaSourceDevMode::FromStorageLocation(
                       profile()->GetPath(), app->isolation_data()->location()),
                   [&](auto error) {
                     std::move(callback).Run("can only update dev-mode apps");
                   });

  auto url_info = IsolatedWebAppUrlInfo::Create(app->manifest_id());
  if (!url_info.has_value()) {
    std::move(callback).Run("unable to create UrlInfo from start url");
    return;
  }

  auto& manager = provider->iwa_update_manager();
  manager.DiscoverApplyAndPrioritizeLocalDevModeUpdate(
      location.has_value() ? *location
                           : IwaSourceDevModeWithFileOp(
                                 source.WithFileOp(kDefaultBundleDevFileOp)),
      *url_info,
      base::BindOnce([](base::expected<base::Version, std::string> result) {
        if (result.has_value()) {
          return base::StrCat(
              {"Update to version ", result->GetString(),
               " successful (refresh this page to reflect the update)."});
        }
        return "Update failed: " + result.error();
      }).Then(std::move(callback)));
}

void IwaInternalsHandler::RotateKey(
    const std::string& web_bundle_id,
    const std::optional<std::vector<uint8_t>>& public_key) {
  IwaKeyDistributionInfoProvider::GetInstance()->RotateKeyForDevMode(
      base::PassKey<IwaInternalsHandler>(), web_bundle_id, public_key);
}

void IwaInternalsHandler::UpdateManifestInstalledIsolatedWebApp(
    const webapps::AppId& app_id,
    Handler::UpdateManifestInstalledIsolatedWebAppCallback callback) {
  if (!update_handler_) {
    std::move(callback).Run(
        "WebAppProvider is not available for the current profile.");
    return;
  }
  update_handler_->UpdateManifestInstalledIsolatedWebApp(app_id,
                                                         std::move(callback));
}

void IwaInternalsHandler::DownloadWebBundleToFile(
    const GURL& web_bundle_url,
    const GURL& update_manifest_url,
    Handler::InstallIsolatedWebAppFromBundleUrlCallback callback,
    ScopedTempWebBundleFile file) {
  if (!file) {
    std::move(callback).Run(::mojom::InstallIsolatedWebAppResult::NewError(
        "Couldn't create file."));
    return;
  }
  auto path = file.path();

  auto downloader = std::make_unique<IsolatedWebAppDownloader>(
      profile()->GetURLLoaderFactory());
  auto* downloader_ptr = downloader.get();
  base::OnceClosure downloader_keep_alive =
      base::DoNothingWithBoundArgs(std::move(downloader));

  downloader_ptr->DownloadSignedWebBundle(
      web_bundle_url, std::move(path), kDownloadWebBundleAnnotation,
      base::BindOnce(&IwaInternalsHandler::OnWebBundleDownloaded,
                     weak_ptr_factory_.GetWeakPtr(), update_manifest_url,
                     std::move(callback), std::move(file))
          .Then(std::move(downloader_keep_alive)));
}

void IwaInternalsHandler::OnWebBundleDownloaded(
    const GURL& update_manifest_url,
    Handler::InstallIsolatedWebAppFromBundleUrlCallback callback,
    ScopedTempWebBundleFile bundle,
    int32_t result) {
  if (result != net::OK) {
    std::move(callback).Run(::mojom::InstallIsolatedWebAppResult::NewError(
        base::StrCat({"Network error while downloading bundle file: ",
                      base::ToString(result)})));
    return;
  }

  const base::ScopedTempFile* file = bundle.file();
  base::OnceClosure bundle_keep_alive =
      base::DoNothingWithBoundArgs(std::move(bundle));

  WebAppProvider::GetForWebApps(profile())
      ->isolated_web_app_installation_manager()
      .InstallIsolatedWebAppFromDevModeBundle(
          file, IsolatedWebAppInstallationManager::InstallSurface::kDevUi,
          base::BindOnce(
              &IwaInternalsHandler::
                  OnInstalledIsolatedWebAppInDevModeFromWebBundle,
              weak_ptr_factory_.GetWeakPtr(), update_manifest_url,
              std::move(callback).Then(std::move(bundle_keep_alive))));
}

void IwaInternalsHandler::OnInstalledIsolatedWebAppInDevModeFromWebBundle(
    const GURL& update_manifest_url,
    base::OnceCallback<void(::mojom::InstallIsolatedWebAppResultPtr)> callback,
    base::expected<InstallIsolatedWebAppCommandSuccess, std::string> result) {
  ASSIGN_OR_RETURN(auto install_info, std::move(result),
                   [&](const std::string& error) {
                     std::move(callback).Run(
                         ::mojom::InstallIsolatedWebAppResult::NewError(error));
                   });

  web_app::WebAppProvider::GetForWebApps(&profile_.get())
      ->scheduler()
      .ScheduleCallbackWithResult(
          "WebAppInternalsHandler::SetUpdateManifestUrl",
          web_app::AppLockDescription(install_info.url_info.app_id()),
          base::BindOnce(
              [](const IsolatedWebAppUrlInfo& url_info,
                 const GURL& update_manifest_url, AppLock& lock,
                 base::Value::Dict& debug_value) {
                web_app::ScopedRegistryUpdate update =
                    lock.sync_bridge().BeginUpdate();

                web_app::WebApp* web_app = update->UpdateApp(url_info.app_id());
                if (!web_app || !web_app->isolation_data()) {
                  return ::mojom::InstallIsolatedWebAppResult::NewError(
                      "Something went wrong while setting the update manifest "
                      "url.");
                }
                web_app->SetIsolationData(
                    web_app::IsolationData::Builder(*web_app->isolation_data())
                        .SetUpdateManifestUrl(update_manifest_url)
                        .Build());

                auto success = ::mojom::InstallIsolatedWebAppSuccess::New();
                success->web_bundle_id = url_info.web_bundle_id().id();

                return ::mojom::InstallIsolatedWebAppResult::NewSuccess(
                    std::move(success));
              },
              install_info.url_info, update_manifest_url),
          std::move(callback), /*arg_for_shutdown=*/
          ::mojom::InstallIsolatedWebAppResult::NewError(
              "The web app system has shut down."));
}

}  // namespace web_app
