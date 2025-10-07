// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/web_app_internals/iwa_internals_handler.h"

#include <variant>

#include "base/containers/map_util.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected_macros.h"
#include "base/types/optional_util.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/ui/webui/web_app_internals/web_app_internals.mojom.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/isolated_web_app_installation_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolation_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_discovery_task.h"
#include "chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest_fetcher.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "components/tabs/public/tab_interface.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/isolated_web_apps/iwa_key_distribution_info_provider.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/base/base_window.h"

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
      std::optional<IwaVersion> pinned_version,
      bool allow_downgrades,
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

    // Some older installs might not have the `update_channel` field set -- in
    // this case we fall back to the `default` channel.
    // For now, we do not enable setting pinned_version field via iwa internals.
    // By not setting `pinned_version` argument, discovery task defaults to
    // searching for the latest available version on current update channel.
    provider_->iwa_update_manager().DiscoverUpdatesForApp(
        url_info, *isolation_data.update_manifest_url(),
        /*update_channel=*/
        isolation_data.update_channel().value_or(
            UpdateChannel::default_channel()),
        allow_downgrades, pinned_version,
        /*dev_mode=*/true);
  }

  // IsolatedWebAppUpdateManager::Observer:
  void OnUpdateDiscoveryTaskCompleted(
      const webapps::AppId& app_id,
      IsolatedWebAppUpdateDiscoveryTask::CompletionStatus status) override {
    if (status.has_value()) {
      switch (*status) {
        case IsolatedWebAppUpdateDiscoveryTask::Success::
            kUpdateFoundAndSavedInDatabase:
        case IsolatedWebAppUpdateDiscoveryTask::Success::
            kPinnedVersionUpdateFoundAndSavedInDatabase:
        case IsolatedWebAppUpdateDiscoveryTask::Success::
            kDowngradeVersionFoundAndSavedInDatabase:
          // An update has been found and is now pending. Return and wait for
          // OnUpdateApplyTaskCompleted to be called.
          return;
        case IsolatedWebAppUpdateDiscoveryTask::Success::kNoUpdateFound:
        case IsolatedWebAppUpdateDiscoveryTask::Success::kUpdateAlreadyPending:
          // No update will be applied, so we can proceed to call the callback.
          break;
      }
    }

    ASSIGN_OR_RETURN(auto callback, ConsumeUpdateRequest(app_id), [](auto) {});
    if (status.has_value()) {
      std::move(callback).Run(
          "Update skipped: app is already on the latest version or the updates "
          "are disabled due to set `pinned_version` field.");
    } else {
      std::move(callback).Run(
          "Update failed: " +
          IsolatedWebAppUpdateDiscoveryTask::ErrorToString(status.error()));
    }
  }

  void OnUpdateApplyTaskCompleted(
      const webapps::AppId& app_id,
      IsolatedWebAppApplyUpdateCommandResult status) override {
    ASSIGN_OR_RETURN(auto callback, ConsumeUpdateRequest(app_id), [](auto) {});

    ASSIGN_OR_RETURN(
        const WebApp& iwa,
        GetIsolatedWebAppById(provider_->registrar_unsafe(), app_id),
        [&](const std::string& error) { std::move(callback).Run(error); });
    if (status.has_value()) {
      std::move(callback).Run(
          base::StringPrintf("Update to v%s successful (refresh the page "
                             "to reflect the update).",
                             iwa.isolation_data()->version().GetString()));
    } else {
      std::move(callback).Run("Update failed: " + status.error().message);
    }
  }

 private:
  // Retrieves the pending request for `app_id` and erases the entry from the
  // requests map. If there's no matching entry for `app_id`, returns an
  // unexpected.
  base::expected<Handler::UpdateDevProxyIsolatedWebAppCallback, std::monostate>
  ConsumeUpdateRequest(const webapps::AppId& app_id) {
    auto itr = update_requests_.find(app_id);
    if (itr == update_requests_.end()) {
      return base::unexpected(std::monostate());
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
    SendError(std::move(callback),
              "WebAppProvider not supported for current profile.");
    return;
  }
  if (!params->update_info) {
    SendError(std::move(callback),
              "Update info is required for this operation.");
    return;
  }
  if (!params->update_info->update_manifest_url.is_valid()) {
    SendError(std::move(callback), "Update manifest URL is not a valid GURL.");
    return;
  }
  if (params->update_info->update_channel.empty()) {
    SendError(std::move(callback),
              "Update channel is required for this operation.");
    return;
  }

  WebAppProvider::GetForWebApps(profile())
      ->isolated_web_app_installation_manager()
      .DownloadAndInstallIsolatedWebAppFromDevModeBundle(
          params->web_bundle_url,
          IsolatedWebAppInstallationManager::InstallSurface::kDevUi,
          base::BindOnce(&IwaInternalsHandler::
                             OnInstalledIsolatedWebAppInDevModeFromWebBundle,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(params->update_info), std::move(callback)));
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
      IwaSourceBundleDevModeWithFileOp(*path, IwaSourceBundleDevFileOp::kCopy));
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

    base::expected<IwaSourceDevMode, std::monostate> source =
        IwaSourceDevMode::FromStorageLocation(profile()->GetPath(),
                                              app.isolation_data()->location());
    if (!source.has_value()) {
      continue;
    }

    auto signed_web_bundle_id =
        web_package::SignedWebBundleId::Create(app.manifest_id().host());
    CHECK(signed_web_bundle_id.has_value())
        << "Invalid host in manifest_id for IWA: " << app.app_id()
        << " with manifest_id: " << app.manifest_id();

    IwaOrigin iwa_origin(*signed_web_bundle_id);
    std::string web_bundle_id = iwa_origin.web_bundle_id().id();

    const web_app::IsolationData& isolation_data = *app.isolation_data();
    std::optional<std::string> pinned_version;
    if (auto* version_entry =
            base::FindOrNull(pinned_versions_, app.app_id())) {
      pinned_version = version_entry->GetString();
    }
    bool allow_downgrades = app_ids_allowing_downgrades_.contains(app.app_id());
    std::visit(
        absl::Overload{
            [&](const IwaSourceBundleDevMode& source) {
              dev_mode_apps.emplace_back(::mojom::IwaDevModeAppInfo::New(
                  app.app_id(), web_bundle_id, app.untranslated_name(),
                  ::mojom::IwaDevModeLocation::NewBundlePath(source.path()),
                  app.isolation_data()->version().GetString(),
                  /*update_info=*/isolation_data.update_manifest_url()
                      ? ::mojom::UpdateInfo::New(
                            *isolation_data.update_manifest_url(),
                            isolation_data.update_channel()
                                .value_or(UpdateChannel::default_channel())
                                .ToString(),
                            pinned_version, allow_downgrades)
                      : nullptr));
            },
            [&](const IwaSourceProxy& source) {
              dev_mode_apps.emplace_back(::mojom::IwaDevModeAppInfo::New(
                  app.app_id(), web_bundle_id, app.untranslated_name(),
                  ::mojom::IwaDevModeLocation::NewProxyOrigin(
                      source.proxy_url()),
                  app.isolation_data()->version().GetString(),
                  /*update_info=*/nullptr));
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
                           : IwaSourceDevModeWithFileOp(source.WithFileOp(
                                 IwaSourceBundleDevFileOp::kCopy)),
      *url_info,
      base::BindOnce([](base::expected<IwaVersion, std::string> result) {
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
  IwaKeyDistributionInfoProvider::GetInstance().RotateKeyForDevMode(
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
  std::optional<IwaVersion> pinned_version =
      base::OptionalFromPtr(base::FindOrNull(pinned_versions_, app_id));
  bool allow_downgrades = app_ids_allowing_downgrades_.contains(app_id);

  update_handler_->UpdateManifestInstalledIsolatedWebApp(
      app_id, pinned_version, allow_downgrades, std::move(callback));
}

void IwaInternalsHandler::SetUpdateChannelForIsolatedWebApp(
    const webapps::AppId& app_id,
    const std::string& update_channel,
    Handler::SetUpdateChannelForIsolatedWebAppCallback callback) {
  auto* provider = WebAppProvider::GetForWebApps(profile());
  if (!provider) {
    std::move(callback).Run(/*success=*/false);
    return;
  }
  provider->scheduler().ScheduleCallbackWithResult(
      "WebAppInternalsHandler::SetUpdateChannel",
      web_app::AppLockDescription(app_id),
      base::BindOnce(
          [](const webapps::AppId& app_id, const std::string& update_channel,
             AppLock& lock, base::Value::Dict& debug_value) {
            web_app::ScopedRegistryUpdate update =
                lock.sync_bridge().BeginUpdate();

            web_app::WebApp* web_app = update->UpdateApp(app_id);
            auto channel = UpdateChannel::Create(update_channel);
            if (!web_app || !web_app->isolation_data() ||
                !web_app->isolation_data()->update_manifest_url() ||
                !channel.has_value()) {
              return false;
            }

            web_app->SetIsolationData(
                web_app::IsolationData::Builder(*web_app->isolation_data())
                    .SetUpdateChannel(std::move(*channel))
                    .Build());

            return true;
          },
          app_id, update_channel),
      std::move(callback), /*arg_for_shutdown=*/false);
}

void IwaInternalsHandler::SetPinnedVersionForIsolatedWebApp(
    const webapps::AppId& app_id,
    const std::string pinned_version,
    Handler::SetPinnedVersionForIsolatedWebAppCallback callback) {
  auto* provider = WebAppProvider::GetForWebApps(profile());
  if (!provider) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  RETURN_IF_ERROR(GetIsolatedWebAppById(provider->registrar_unsafe(), app_id),
                  [&](auto) { std::move(callback).Run(/*success=*/false); });

  auto version = IwaVersion::Create(pinned_version);
  if (!version.has_value()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  pinned_versions_.insert_or_assign(app_id, *std::move(version));
  std::move(callback).Run(/*success=*/true);
}

void IwaInternalsHandler::ResetPinnedVersionForIsolatedWebApp(
    const webapps::AppId& app_id) {
  pinned_versions_.erase(app_id);
}

void IwaInternalsHandler::SetAllowDowngradesForIsolatedWebApp(
    bool allow_downgrades,
    const webapps::AppId& app_id) {
  auto* provider = WebAppProvider::GetForWebApps(profile());
  if (!provider || provider->registrar_unsafe().GetInstallState(app_id) !=
                       proto::INSTALLED_WITH_OS_INTEGRATION) {
    return;
  }

  // Removes `app_id` for which downgrades were turned off.
  if (base::Contains(app_ids_allowing_downgrades_, app_id) &&
      !allow_downgrades) {
    app_ids_allowing_downgrades_.erase(app_id);
    return;
  }
  app_ids_allowing_downgrades_.insert(app_id);
}

void IwaInternalsHandler::DeleteIsolatedWebApp(
    const webapps::AppId& app_id,
    Handler::DeleteIsolatedWebAppCallback callback) {
  auto* provider = WebAppProvider::GetForWebApps(profile());

  // Native Window required for the dialog box
  gfx::NativeWindow native_window = GetHostingNativeWindow();

  provider->ui_manager().PresentUserUninstallDialog(
      app_id, webapps::WebappUninstallSource::kAppsPage, native_window,
      base::BindOnce([](webapps::UninstallResultCode code) {
        return webapps::UninstallSucceeded(code);
      }).Then(std::move(callback)));
}

gfx::NativeWindow IwaInternalsHandler::GetHostingNativeWindow() {
  content::WebContents* web_contents = web_ui_->GetWebContents();

  tabs::TabInterface* tab_interface =
      tabs::TabInterface::GetFromContents(web_contents);
  CHECK(tab_interface);

  BrowserWindowInterface* browser_window_interface =
      tab_interface->GetBrowserWindowInterface();
  CHECK(browser_window_interface);

  ui::BaseWindow* base_window = browser_window_interface->GetWindow();
  CHECK(base_window);

  gfx::NativeWindow native_window = base_window->GetNativeWindow();
  CHECK(native_window);

  return native_window;
}

void IwaInternalsHandler::OnInstalledIsolatedWebAppInDevModeFromWebBundle(
    ::mojom::UpdateInfoPtr update_info,
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
          "WebAppInternalsHandler::SetUpdateInfo",
          web_app::AppLockDescription(install_info.url_info.app_id()),
          base::BindOnce(
              [](const IsolatedWebAppUrlInfo& url_info,
                 ::mojom::UpdateInfoPtr update_info, AppLock& lock,
                 base::Value::Dict& debug_value) {
                web_app::ScopedRegistryUpdate update =
                    lock.sync_bridge().BeginUpdate();

                web_app::WebApp* web_app = update->UpdateApp(url_info.app_id());
                if (!web_app || !web_app->isolation_data()) {
                  return ::mojom::InstallIsolatedWebAppResult::NewError(
                      "Something went wrong while setting the update info.");
                }

                auto update_channel =
                    UpdateChannel::Create(update_info->update_channel);
                if (!update_channel.has_value()) {
                  return ::mojom::InstallIsolatedWebAppResult::NewError(
                      "Something went wrong while setting the update "
                      "channel.");
                }

                GURL update_manifest_url = update_info->update_manifest_url;
                if (!update_manifest_url.is_valid()) {
                  return ::mojom::InstallIsolatedWebAppResult::NewError(
                      "Something went wrong while setting the update "
                      "manifest url.");
                }
                web_app->SetIsolationData(
                    web_app::IsolationData::Builder(*web_app->isolation_data())
                        .SetUpdateManifestUrl(update_info->update_manifest_url)
                        .SetUpdateChannel(std::move(*update_channel))
                        .Build());

                auto success = ::mojom::InstallIsolatedWebAppSuccess::New();
                success->web_bundle_id = url_info.web_bundle_id().id();

                return ::mojom::InstallIsolatedWebAppResult::NewSuccess(
                    std::move(success));
              },
              install_info.url_info, std::move(update_info)),
          std::move(callback), /*arg_for_shutdown=*/
          ::mojom::InstallIsolatedWebAppResult::NewError(
              "The web app system has shut down."));
}

}  // namespace web_app
