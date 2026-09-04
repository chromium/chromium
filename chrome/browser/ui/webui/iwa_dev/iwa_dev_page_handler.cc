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
#include "base/strings/strcat.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_check_and_prepare_task.h"
#include "chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_manager.h"
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
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "components/webapps/isolated_web_apps/types/source.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"
#include "components/webapps/isolated_web_apps/types/update_check_and_prepare_result.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/mojom/base/empty.mojom.h"
#include "mojo/public/mojom/base/error.mojom.h"
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

template <typename T>
base::expected<std::monostate, mojo_base::mojom::ErrorPtr>
MapToMojomEmptyResult(base::expected<T, std::string> result) {
  if (!result.has_value()) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInvalidArgument, result.error()));
  }
  return std::monostate();
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

bool UpdateFound(web_app::IwaUpdateCheckAndPrepareSuccess status) {
  switch (status) {
    case web_app::IwaUpdateCheckAndPrepareSuccess::
        kUpdateFoundAndSavedInDatabase:
    case web_app::IwaUpdateCheckAndPrepareSuccess::
        kPinnedVersionUpdateFoundAndSavedInDatabase:
    case web_app::IwaUpdateCheckAndPrepareSuccess::
        kDowngradeVersionFoundAndSavedInDatabase:
    case web_app::IwaUpdateCheckAndPrepareSuccess::kUpdateAlreadyPending:
      return true;
    case web_app::IwaUpdateCheckAndPrepareSuccess::kNoUpdateFound:
    case web_app::IwaUpdateCheckAndPrepareSuccess::kUpdateFound:
      return false;
  }
}

}  // namespace

class IwaDevPageHandler::LocalBundleSelectListener
    : public content::FileSelectListener {
 public:
  using Callback = base::OnceCallback<void(
      base::expected<base::FilePath, mojo_base::mojom::ErrorPtr>)>;

  explicit LocalBundleSelectListener(Callback callback)
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

    const base::FilePath& file_path = file.get_native_file()->file_path;
    if (!file_path.MatchesExtension(FILE_PATH_LITERAL(".swbn"))) {
      std::move(callback_).Run(base::unexpected(
          mojo_base::mojom::Error::New(mojo_base::mojom::Code::kInvalidArgument,
                                       "Invalid file type. Please select a "
                                       "Signed Web Bundle (.swbn) file.")));
      return;
    }

    std::move(callback_).Run(file_path);
  }

  void FileSelectionCanceled() override {
    CHECK(callback_);
    std::move(callback_).Run(base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInvalidArgument, "No file selected")));
  }

 private:
  ~LocalBundleSelectListener() override = default;

  Callback callback_;
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
  update_observation_.Observe(&provider_->isolated_web_app_update_manager());
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
          base::BindOnce(&MapToMojomEmptyResult<
                             web_app::InstallIsolatedWebAppCommandSuccess>)
              .Then(std::move(callback)));
}

void IwaDevPageHandler::InstallAppFromUpdateManifest(
    const GURL& web_bundle_url,
    iwa_dev::mojom::UpdateInfoPtr update_info,
    InstallAppFromUpdateManifestCallback callback) {
  if (!web_bundle_url.SchemeIsHTTPOrHTTPS()) {
    std::move(callback).Run(base::unexpected(
        mojo_base::mojom::Error::New(mojo_base::mojom::Code::kInvalidArgument,
                                     "Invalid Web Bundle URL provided.")));
    return;
  }
  if (!update_info->update_manifest_url.SchemeIsHTTPOrHTTPS()) {
    std::move(callback).Run(base::unexpected(
        mojo_base::mojom::Error::New(mojo_base::mojom::Code::kInvalidArgument,
                                     "Invalid Update Manifest URL provided.")));
    return;
  }
  ASSIGN_OR_RETURN(
      web_app::UpdateChannel update_channel,
      web_app::UpdateChannel::Create(update_info->update_channel), [&](auto) {
        std::move(callback).Run(base::unexpected(mojo_base::mojom::Error::New(
            mojo_base::mojom::Code::kInvalidArgument,
            "Invalid update channel provided.")));
      });

  provider_->isolated_web_app_dev_install_manager()
      .DownloadAndInstallIsolatedWebAppFromDevModeBundle(
          web_bundle_url,
          web_app::IsolatedWebAppDevInstallManager::InstallSurface::kDevUi,
          base::BindOnce(&MapToMojomEmptyResult<
                             web_app::InstallIsolatedWebAppCommandSuccess>)
              .Then(std::move(callback)),
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
                            web_app::UpdateManifestFetcher::Error> result)
              -> base::expected<iwa_dev::mojom::UpdateManifestPtr,
                                mojo_base::mojom::ErrorPtr> {
            if (!result.has_value()) {
              return base::unexpected(mojo_base::mojom::Error::New(
                  mojo_base::mojom::Code::kInvalidArgument,
                  base::StrCat({"Manifest fetch failed: ",
                                web_app::UpdateManifestFetcher::ErrorToString(
                                    result.error())})));
            }
            return MapToMojomUpdateManifest(result.value());
          })
          .Then(std::move(callback))
          .Then(std::move(fetcher_keep_alive)));
}

void IwaDevPageHandler::SelectLocalWebBundle(
    base::OnceCallback<void(SelectLocalBundleResult)> callback) {
  content::RenderFrameHost* render_frame_host =
      web_contents_->GetPrimaryMainFrame();

  base::MakeRefCounted<LocalBundleSelectListener>(std::move(callback))
      ->Show(render_frame_host);
}

void IwaDevPageHandler::SelectAndInstallAppFromLocalWebBundle(
    SelectAndInstallAppFromLocalWebBundleCallback callback) {
  SelectLocalWebBundle(
      base::BindOnce(&IwaDevPageHandler::OnLocalBundleSelectedForInstall,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void IwaDevPageHandler::OnLocalBundleSelectedForInstall(
    SelectAndInstallAppFromLocalWebBundleCallback callback,
    SelectLocalBundleResult path) {
  if (!path.has_value()) {
    std::move(callback).Run(base::unexpected(std::move(path.error())));
    return;
  }

  provider_->isolated_web_app_dev_install_manager()
      .InstallIsolatedWebAppFromDevModeBundle(
          *path,
          web_app::IsolatedWebAppDevInstallManager::InstallSurface::kDevUi,
          base::BindOnce(&MapToMojomEmptyResult<
                             web_app::InstallIsolatedWebAppCommandSuccess>)
              .Then(std::move(callback)));
}

void IwaDevPageHandler::SelectAndUpdateAppFromLocalWebBundle(
    const std::string& app_id,
    SelectAndUpdateAppFromLocalWebBundleCallback callback) {
  SelectLocalWebBundle(base::BindOnce(
      &IwaDevPageHandler::OnLocalBundleSelectedForUpdate,
      weak_ptr_factory_.GetWeakPtr(), app_id, std::move(callback)));
}

void IwaDevPageHandler::OnLocalBundleSelectedForUpdate(
    const std::string& app_id,
    SelectAndUpdateAppFromLocalWebBundleCallback callback,
    SelectLocalBundleResult path) {
  if (!path.has_value()) {
    std::move(callback).Run(base::unexpected(std::move(path.error())));
    return;
  }

  ApplyDevModeUpdate(app_id,
                     web_app::IwaSourceDevModeWithFileOp(
                         web_app::IwaSourceBundleDevModeWithFileOp(
                             *path, web_app::IwaSourceBundleDevFileOp::kCopy)),
                     std::move(callback));
}

void IwaDevPageHandler::SetUpdateChannel(const std::string& app_id,
                                         const std::string& update_channel,
                                         SetUpdateChannelCallback callback) {
  ASSIGN_OR_RETURN(
      web_app::UpdateChannel channel,
      web_app::UpdateChannel::Create(update_channel), [&](auto) {
        std::move(callback).Run(base::unexpected(mojo_base::mojom::Error::New(
            mojo_base::mojom::Code::kInvalidArgument,
            "Invalid update channel provided.")));
      });

  provider_->scheduler().ScheduleCallbackWithResult(
      "IwaDevPageHandler::SetUpdateChannel",
      web_app::AppLockDescription(app_id),
      base::BindOnce(
          [](const webapps::AppId& app_id, web_app::UpdateChannel channel,
             web_app::AppLock& lock, base::DictValue&)
              -> base::expected<std::monostate, mojo_base::mojom::ErrorPtr> {
            const web_app::WebApp* iwa = lock.registrar().GetAppById(
                app_id, web_app::WebAppFilter::IsDevModeIsolatedApp());
            if (!iwa) {
              return base::unexpected(mojo_base::mojom::Error::New(
                  mojo_base::mojom::Code::kNotFound, "App not found."));
            }

            const web_app::IsolationData& isolation_data =
                *iwa->isolation_data();
            if (!isolation_data.update_manifest_url()) {
              return base::unexpected(mojo_base::mojom::Error::New(
                  mojo_base::mojom::Code::kFailedPrecondition,
                  "App was not installed from an update manifest."));
            }

            if (isolation_data.update_channel() == channel) {
              return std::monostate();
            }

            {
              // ScopedRegistryUpdate commits on destruction before observers
              // are notified.
              web_app::ScopedRegistryUpdate update =
                  lock.sync_bridge().BeginUpdate();
              update->UpdateApp(app_id)->SetIsolationData(
                  web_app::IsolationData::Builder(isolation_data)
                      .SetUpdateChannel(std::move(channel))
                      .Build());
            }
            lock.install_manager().NotifyWebAppManifestUpdated(app_id);
            return std::monostate();
          },
          app_id, std::move(channel)),
      std::move(callback),
      /*arg_for_shutdown=*/
      base::expected<std::monostate, mojo_base::mojom::ErrorPtr>(
          base::unexpected(mojo_base::mojom::Error::New(
              mojo_base::mojom::Code::kAborted, "Operation aborted."))));
}

void IwaDevPageHandler::UpdateDevProxyInstalledApp(
    const std::string& app_id,
    UpdateDevProxyInstalledAppCallback callback) {
  ApplyDevModeUpdate(app_id, /*location=*/std::nullopt, std::move(callback));
}

void IwaDevPageHandler::UpdateManifestInstalledApp(
    const std::string& app_id,
    iwa_dev::mojom::UpdateManifestOptionsPtr options,
    UpdateManifestInstalledAppCallback callback) {
  if (manifest_update_requests_.contains(app_id)) {
    std::move(callback).Run(base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInvalidArgument,
        "Please wait for the pending update request to resolve first.")));
    return;
  }

  ASSIGN_OR_RETURN(
      const web_app::WebApp* iwa, GetInstalledAppById(app_id),
      [&](mojo_base::mojom::ErrorPtr error) {
        std::move(callback).Run(base::unexpected(std::move(error)));
      });

  const web_app::IsolationData& isolation_data = *iwa->isolation_data();
  if (!isolation_data.update_manifest_url()) {
    std::move(callback).Run(base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInvalidArgument,
        "Only dev-mode apps with update_manifest_url set can be updated via "
        "this routine.")));
    return;
  }

  std::optional<web_app::IwaVersion> pinned_version;
  if (options->pinned_version) {
    ASSIGN_OR_RETURN(
        pinned_version, web_app::IwaVersion::Create(*options->pinned_version),
        [&](auto) {
          std::move(callback).Run(base::unexpected(mojo_base::mojom::Error::New(
              mojo_base::mojom::Code::kInvalidArgument,
              "Invalid pinned version provided.")));
        });
  }

  manifest_update_requests_.emplace(app_id, std::move(callback));

  provider_->isolated_web_app_update_manager().DiscoverAndPrepareUpdate(
      *web_app::IsolatedWebAppUrlInfo::Create(iwa->scope()),
      *isolation_data.update_manifest_url(),
      /*update_channel=*/
      isolation_data.update_channel().value_or(
          web_app::UpdateChannel::default_channel()),
      /*allow_downgrades=*/options->allow_downgrades,
      /*pinned_version=*/std::move(pinned_version),
      /*dev_mode=*/true);
}

void IwaDevPageHandler::OnUpdateDiscoverAndPrepareTaskCompleted(
    const webapps::AppId& app_id,
    web_app::IwaUpdateCheckAndPrepareResult result) {
  ASSIGN_OR_RETURN(
      web_app::IwaUpdateCheckAndPrepareSuccess status, result,
      [&](web_app::IwaUpdateCheckAndPrepareError error) {
        auto callback = TakeManifestUpdateRequest(app_id);
        if (callback) {
          std::move(*callback).Run(
              base::unexpected(mojo_base::mojom::Error::New(
                  mojo_base::mojom::Code::kInvalidArgument,
                  web_app::IwaUpdateCheckAndPrepareErrorToString(error))));
        }
      });

  if (UpdateFound(status)) {
    return;
  }

  auto callback = TakeManifestUpdateRequest(app_id);
  if (callback) {
    std::move(*callback).Run(base::unexpected(
        mojo_base::mojom::Error::New(mojo_base::mojom::Code::kInvalidArgument,
                                     "App is already on the latest version.")));
  }
}

void IwaDevPageHandler::OnUpdateApplyTaskCompleted(
    const webapps::AppId& app_id,
    web_app::IsolatedWebAppApplyUpdateCommandResult status) {
  auto callback = TakeManifestUpdateRequest(app_id);
  if (!callback) {
    return;
  }

  RETURN_IF_ERROR(
      GetInstalledAppById(app_id), [&](mojo_base::mojom::ErrorPtr error) {
        std::move(*callback).Run(base::unexpected(std::move(error)));
      });

  if (status.has_value()) {
    std::move(*callback).Run(std::monostate());
  } else {
    std::move(*callback).Run(base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInvalidArgument, status.error().message)));
  }
}

std::optional<IwaDevPageHandler::UpdateManifestInstalledAppCallback>
IwaDevPageHandler::TakeManifestUpdateRequest(const webapps::AppId& app_id) {
  auto itr = manifest_update_requests_.find(app_id);
  if (itr == manifest_update_requests_.end()) {
    return std::nullopt;
  }
  auto callback = std::move(itr->second);
  manifest_update_requests_.erase(itr);
  return callback;
}

base::expected<const web_app::WebApp*, mojo_base::mojom::ErrorPtr>
IwaDevPageHandler::GetInstalledAppById(const std::string& app_id) {
  const web_app::WebApp* iwa = provider_->registrar_unsafe().GetAppById(
      app_id, web_app::WebAppFilter::IsDevModeIsolatedApp());
  if (!iwa) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInvalidArgument, "App not found."));
  }
  return iwa;
}

void IwaDevPageHandler::ApplyDevModeUpdate(
    const std::string& app_id,
    base::optional_ref<const web_app::IwaSourceDevModeWithFileOp> location,
    base::OnceCallback<void(
        base::expected<std::monostate, mojo_base::mojom::ErrorPtr>)> callback) {
  ASSIGN_OR_RETURN(
      const web_app::WebApp* iwa, GetInstalledAppById(app_id),
      [&](mojo_base::mojom::ErrorPtr error) {
        std::move(callback).Run(base::unexpected(std::move(error)));
      });

  ASSIGN_OR_RETURN(
      web_app::IwaSourceDevMode source,
      web_app::IwaSourceDevMode::FromStorageLocation(
          profile_->GetPath(), iwa->isolation_data()->location()),
      [&](const auto& err) {
        std::move(callback).Run(base::unexpected(mojo_base::mojom::Error::New(
            mojo_base::mojom::Code::kUnknown, "Invalid storage location")));
      });

  auto url_info =
      web_app::IsolatedWebAppUrlInfo::Create(iwa->manifest_id().value());
  if (!url_info.has_value()) {
    std::move(callback).Run(base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInvalidArgument,
        "Unable to create UrlInfo from start url.")));
    return;
  }

  provider_->isolated_web_app_update_manager()
      .DiscoverApplyAndPrioritizeLocalDevModeUpdate(
          location.has_value()
              ? *location
              : web_app::IwaSourceDevModeWithFileOp(source.WithFileOp(
                    web_app::IwaSourceBundleDevFileOp::kCopy)),
          *url_info,
          base::BindOnce(&MapToMojomEmptyResult<web_app::IwaVersion>)
              .Then(std::move(callback)));
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
