// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/webapps/browser/installable/installable_params.h"
#include "components/webapps/common/web_page_metadata.mojom.h"
#include "components/webapps/common/web_page_metadata_agent.mojom.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {

WebAppDataRetriever::WebAppDataRetriever() = default;

WebAppDataRetriever::~WebAppDataRetriever() = default;

void WebAppDataRetriever::GetWebAppInstallInfo(
    content::WebContents* web_contents,
    GetWebAppInstallInfoCallback callback) {
  DCHECK(!web_contents->IsBeingDestroyed());
  Observe(web_contents);

  // Concurrent calls are not allowed.
  DCHECK(!get_web_app_info_callback_);
  get_web_app_info_callback_ = std::move(callback);

  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (entry->IsInitialEntry()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&WebAppDataRetriever::CallCallbackOnError,
                       weak_ptr_factory_.GetWeakPtr(), absl::nullopt));
    return;
  }

  // Makes a copy of WebContents fields right after Commit but before a mojo
  // request to the renderer process.
  fallback_install_info_ = std::make_unique<WebAppInstallInfo>(
      GenerateManifestIdFromStartUrlOnly(web_contents->GetLastCommittedURL()));
  fallback_install_info_->start_url = web_contents->GetLastCommittedURL();
  fallback_install_info_->title = web_contents->GetTitle();
  if (fallback_install_info_->title.empty()) {
    fallback_install_info_->title =
        base::UTF8ToUTF16(fallback_install_info_->start_url.spec());
  }

  mojo::AssociatedRemote<webapps::mojom::WebPageMetadataAgent> metadata_agent;
  web_contents->GetPrimaryMainFrame()
      ->GetRemoteAssociatedInterfaces()
      ->GetInterface(&metadata_agent);

  // Set the error handler so that we can run |get_web_app_info_callback_| if
  // the WebContents or the RenderFrameHost are destroyed and the connection
  // to ChromeRenderFrame is lost.
  metadata_agent.set_disconnect_handler(base::BindOnce(
      &WebAppDataRetriever::CallCallbackOnError, weak_ptr_factory_.GetWeakPtr(),
      webapps::InstallableStatusCode::RENDERER_CANCELLED));
  // Bind the InterfacePtr into the callback so that it's kept alive
  // until there's either a connection error or a response.
  auto* web_page_metadata_proxy = metadata_agent.get();
  web_page_metadata_proxy->GetWebPageMetadata(
      base::BindOnce(&WebAppDataRetriever::OnGetWebPageMetadata,
                     weak_ptr_factory_.GetWeakPtr(), std::move(metadata_agent),
                     entry->GetUniqueID()));
}

void WebAppDataRetriever::CheckInstallabilityAndRetrieveManifest(
    content::WebContents* web_contents,
    bool bypass_service_worker_check,
    CheckInstallabilityCallback callback,
    absl::optional<webapps::InstallableParams> params) {
  DCHECK(!web_contents->IsBeingDestroyed());
  webapps::InstallableManager* installable_manager =
      webapps::InstallableManager::FromWebContents(web_contents);
  DCHECK(installable_manager);

  Observe(web_contents);

  // Concurrent calls are not allowed.
  DCHECK(!check_installability_callback_);
  check_installability_callback_ = std::move(callback);

  // TODO(crbug.com/829232) Unify with other calls to GetData.
  if (!params.has_value()) {
    webapps::InstallableParams data_params;
    data_params.check_eligibility = true;
    data_params.valid_primary_icon = true;
    data_params.valid_manifest = true;
    data_params.check_webapp_manifest_display = false;
    // Do not wait for a service worker if it doesn't exist.
    data_params.has_worker = !bypass_service_worker_check;
    params = data_params;
  }
  // Do not wait_for_worker. OnDidPerformInstallableCheck is always invoked.
  installable_manager->GetData(
      params.value(),
      base::BindOnce(&WebAppDataRetriever::OnDidPerformInstallableCheck,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebAppDataRetriever::GetIcons(content::WebContents* web_contents,
                                   base::flat_set<GURL> icon_urls,
                                   bool skip_page_favicons,
                                   GetIconsCallback callback) {
  DCHECK(!web_contents->IsBeingDestroyed());
  Observe(web_contents);

  // Concurrent calls are not allowed.
  CHECK(!get_icons_callback_);
  get_icons_callback_ = std::move(callback);

  IconDownloaderOptions options = {.skip_page_favicons = skip_page_favicons};
  icon_downloader_ = std::make_unique<WebAppIconDownloader>(
      web_contents, std::move(icon_urls),
      base::BindOnce(&WebAppDataRetriever::OnIconsDownloaded,
                     weak_ptr_factory_.GetWeakPtr()),
      options);

  icon_downloader_->Start();
}

void WebAppDataRetriever::WebContentsDestroyed() {
  Observe(nullptr);

  // Avoid initiating new work during web contents destruction.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&WebAppDataRetriever::CallCallbackOnError,
                     weak_ptr_factory_.GetWeakPtr(),
                     webapps::InstallableStatusCode::RENDERER_CANCELLED));
}

void WebAppDataRetriever::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  CallCallbackOnError(webapps::InstallableStatusCode::RENDERER_CANCELLED);
}

void WebAppDataRetriever::OnGetWebPageMetadata(
    mojo::AssociatedRemote<webapps::mojom::WebPageMetadataAgent> metadata_agent,
    int last_committed_nav_entry_unique_id,
    webapps::mojom::WebPageMetadataPtr web_page_metadata) {
  if (ShouldStopRetrieval()) {
    CallCallbackOnError(webapps::InstallableStatusCode::RENDERER_CANCELLED);
    return;
  }

  DCHECK(fallback_install_info_);

  content::WebContents* contents = web_contents();
  Observe(nullptr);

  std::unique_ptr<WebAppInstallInfo> info;

  content::NavigationEntry* entry =
      contents->GetController().GetLastCommittedEntry();

  if (!entry->IsInitialEntry()) {
    if (entry->GetUniqueID() == last_committed_nav_entry_unique_id) {
      info = std::make_unique<WebAppInstallInfo>(*web_page_metadata);
      if (info->manifest_id.is_empty()) {
        info->manifest_id = std::move(fallback_install_info_->manifest_id);
      }
      if (info->start_url.is_empty()) {
        info->start_url = std::move(fallback_install_info_->start_url);
      }
      if (info->title.empty()) {
        info->title = std::move(fallback_install_info_->title);
      }
    } else {
      // WebContents navigation state changed during the call. Ignore the mojo
      // request result. Use default initial info instead.
      info = std::move(fallback_install_info_);
    }
  }

  fallback_install_info_.reset();

  DCHECK(!get_web_app_info_callback_.is_null());
  std::move(get_web_app_info_callback_).Run(std::move(info));
}

void WebAppDataRetriever::OnDidPerformInstallableCheck(
    const webapps::InstallableData& data) {
  if (ShouldStopRetrieval()) {
    CallCallbackOnError(webapps::InstallableStatusCode::RENDERER_CANCELLED);
    return;
  }

  Observe(nullptr);

  const bool is_installable = data.NoBlockingErrors();
  DCHECK(!is_installable || data.valid_manifest);

  blink::mojom::ManifestPtr opt_manifest;
  if (!blink::IsEmptyManifest(*data.manifest)) {
    opt_manifest = data.manifest->Clone();
  }

  DCHECK(!check_installability_callback_.is_null());
  std::move(check_installability_callback_)
      .Run(std::move(opt_manifest), *data.manifest_url, data.valid_manifest,
           data.FirstNoBlockingError());
}

void WebAppDataRetriever::OnIconsDownloaded(
    IconsDownloadedResult result,
    IconsMap icons_map,
    DownloadedIconsHttpResults icons_http_results) {
  if (ShouldStopRetrieval()) {
    CallCallbackOnError(webapps::InstallableStatusCode::RENDERER_CANCELLED);
    return;
  }

  Observe(nullptr);
  icon_downloader_.reset();

  DCHECK(!get_icons_callback_.is_null());
  std::move(get_icons_callback_)
      .Run(result, std::move(icons_map), std::move(icons_http_results));
}

void WebAppDataRetriever::CallCallbackOnError(
    absl::optional<webapps::InstallableStatusCode> error_code) {
  Observe(nullptr);
  DCHECK(ShouldStopRetrieval());
  icon_downloader_.reset();
  fallback_install_info_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Call a callback as a tail call. The callback may destroy |this|.
  if (get_web_app_info_callback_) {
    std::move(get_web_app_info_callback_).Run(nullptr);
  } else if (check_installability_callback_) {
    std::move(check_installability_callback_)
        .Run(/*manifest=*/nullptr, /*manifest_url=*/GURL(),
             /*valid_manifest_for_web_app=*/false,
             /*error_code=*/
             error_code.value_or(
                 webapps::InstallableStatusCode::NO_ERROR_DETECTED));
  } else if (get_icons_callback_) {
    std::move(get_icons_callback_)
        .Run(IconsDownloadedResult::kPrimaryPageChanged, IconsMap{},
             DownloadedIconsHttpResults{});
  }
}

bool WebAppDataRetriever::ShouldStopRetrieval() const {
  return !web_contents() || web_contents()->IsBeingDestroyed();
}

}  // namespace web_app
