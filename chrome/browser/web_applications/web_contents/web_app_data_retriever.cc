// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/webapps/browser/installable/installable_params.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/common/web_page_metadata.mojom.h"
#include "components/webapps/common/web_page_metadata_agent.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {

// static
void WebAppDataRetriever::PopulateWebAppInfoFromMetadata(
    WebAppInstallInfo* info,
    const webapps::mojom::WebPageMetadata& metadata) {
  CHECK(info);
  if (!metadata.application_name.empty()) {
    info->title = metadata.application_name;
  }
  if (!metadata.description.empty()) {
    info->description = metadata.description;
  }
  if (metadata.application_url.is_valid()) {
    const GURL& start_url = metadata.application_url;
    info->SetManifestIdAndStartUrl(
        web_app::GenerateManifestIdFromStartUrlOnly(start_url), start_url);
  }

  for (const auto& icon : metadata.icons) {
    apps::IconInfo icon_info;
    icon_info.url = icon->url;
    if (icon->square_size_px > 0) {
      icon_info.square_size_px = icon->square_size_px;
    }
    info->manifest_icons.push_back(icon_info);
  }
  switch (metadata.mobile_capable) {
    case webapps::mojom::WebPageMobileCapable::UNSPECIFIED:
      info->mobile_capable = WebAppInstallInfo::MOBILE_CAPABLE_UNSPECIFIED;
      break;
    case webapps::mojom::WebPageMobileCapable::ENABLED:
      info->mobile_capable = WebAppInstallInfo::MOBILE_CAPABLE;
      break;
    case webapps::mojom::WebPageMobileCapable::ENABLED_APPLE:
      info->mobile_capable = WebAppInstallInfo::MOBILE_CAPABLE_APPLE;
      break;
  }
}

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

  if (ShouldStopRetrieval()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&WebAppDataRetriever::CallCallbackOnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       webapps::InstallableStatusCode::RENDERER_CANCELLED));
    return;
  }

  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (entry->IsInitialEntry()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&WebAppDataRetriever::CallCallbackOnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       webapps::InstallableStatusCode::RENDERER_CANCELLED));
    return;
  }

  // Makes a copy of WebContents fields right after Commit but before a mojo
  // request to the renderer process.
  GURL start_url = web_contents->GetLastCommittedURL();
  webapps::ManifestId manifest_id =
      GenerateManifestIdFromStartUrlOnly(start_url);
  fallback_install_info_ =
      std::make_unique<WebAppInstallInfo>(manifest_id, start_url);
  fallback_install_info_->title = web_contents->GetTitle();
  if (fallback_install_info_->title.empty()) {
    fallback_install_info_->title = base::UTF8ToUTF16(start_url.spec());
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
    CheckInstallabilityCallback callback,
    std::optional<webapps::InstallableParams> params) {
  DCHECK(!web_contents->IsBeingDestroyed());
  Observe(web_contents);

  // Concurrent calls are not allowed.
  DCHECK(!check_installability_callback_);
  check_installability_callback_ = std::move(callback);
  if (ShouldStopRetrieval()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&WebAppDataRetriever::CallCallbackOnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       webapps::InstallableStatusCode::RENDERER_CANCELLED));
    return;
  }

  // TODO(crbug.com/41380939) Unify with other calls to GetData.
  if (!params.has_value()) {
    webapps::InstallableParams data_params;
    data_params.check_eligibility = true;
    data_params.valid_primary_icon = true;
    data_params.installable_criteria =
        webapps::InstallableCriteria::kValidManifestIgnoreDisplay;
    params = data_params;
  }

  webapps::InstallableManager* installable_manager =
      webapps::InstallableManager::FromWebContents(web_contents);
  DCHECK(installable_manager);

  // Do not wait_for_worker. OnDidPerformInstallableCheck is always invoked.
  installable_manager->GetData(
      params.value(),
      base::BindOnce(&WebAppDataRetriever::OnDidPerformInstallableCheck,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebAppDataRetriever::GetIcons(content::WebContents* web_contents,
                                   const IconUrlSizeSet& extra_icon_urls,
                                   bool skip_page_favicons,
                                   bool fail_all_if_any_fail,
                                   GetIconsCallback callback) {
  DCHECK(!web_contents->IsBeingDestroyed());
  Observe(web_contents);

  // Concurrent calls are not allowed.
  CHECK(!get_icons_callback_);
  get_icons_callback_ = std::move(callback);

  if (ShouldStopRetrieval()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&WebAppDataRetriever::CallCallbackOnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       webapps::InstallableStatusCode::RENDERER_CANCELLED));
    return;
  }

  IconDownloaderOptions options = {
      .skip_page_favicons = skip_page_favicons,
      .fail_all_if_any_fail = fail_all_if_any_fail};
  icon_downloader_ = std::make_unique<WebAppIconDownloader>();
  icon_downloader_->Start(
      web_contents, extra_icon_urls,
      base::BindOnce(&WebAppDataRetriever::OnIconsDownloaded,
                     weak_ptr_factory_.GetWeakPtr()),
      options);
}

void WebAppDataRetriever::WebContentsDestroyed() {
  Observe(nullptr);

  // Avoid initiating new work during web contents destruction.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
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
    webapps::mojom::WebPageMetadataPtr metadata) {
  if (ShouldStopRetrieval()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&WebAppDataRetriever::CallCallbackOnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       webapps::InstallableStatusCode::RENDERER_CANCELLED));
    return;
  }

  DCHECK(fallback_install_info_);

  content::WebContents* contents = web_contents();
  Observe(nullptr);

  content::NavigationEntry* entry =
      contents->GetController().GetLastCommittedEntry();

  CHECK(!get_web_app_info_callback_.is_null());

  if (entry->IsInitialEntry()) {
    // Possibly impossible to get to this state, treat it as an error.
    fallback_install_info_.reset();
    std::move(get_web_app_info_callback_).Run(nullptr);
    return;
  }

  if (entry->GetUniqueID() != last_committed_nav_entry_unique_id) {
    // WebContents navigation state changed during the call. Ignore the mojo
    // request result and use default initial info instead.
    std::move(get_web_app_info_callback_)
        .Run(std::move(fallback_install_info_));
    return;
  }
  CHECK(metadata);

  std::unique_ptr<WebAppInstallInfo> info = std::move(fallback_install_info_);
  PopulateWebAppInfoFromMetadata(info.get(), *metadata);
  std::move(get_web_app_info_callback_).Run(std::move(info));
}

void WebAppDataRetriever::OnDidPerformInstallableCheck(
    const webapps::InstallableData& data) {
  if (ShouldStopRetrieval()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&WebAppDataRetriever::CallCallbackOnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       webapps::InstallableStatusCode::RENDERER_CANCELLED));
    return;
  }

  Observe(nullptr);

  const bool is_installable = data.errors.empty();
  CHECK(!is_installable || data.installable_check_passed);

  blink::mojom::ManifestPtr opt_manifest;
  if (!blink::IsEmptyManifest(*data.manifest)) {
    opt_manifest = data.manifest->Clone();
  }

  CHECK(!check_installability_callback_.is_null());
  std::move(check_installability_callback_)
      .Run(std::move(opt_manifest), data.installable_check_passed,
           data.GetFirstError());
}

void WebAppDataRetriever::OnIconsDownloaded(
    IconsDownloadedResult result,
    IconsMap icons_map,
    DownloadedIconsHttpResults icons_http_results) {
  if (ShouldStopRetrieval()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&WebAppDataRetriever::CallCallbackOnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       webapps::InstallableStatusCode::RENDERER_CANCELLED));
    return;
  }

  Observe(nullptr);
  icon_downloader_.reset();

  DCHECK(!get_icons_callback_.is_null());
  std::move(get_icons_callback_)
      .Run(result, std::move(icons_map), std::move(icons_http_results));
}

void WebAppDataRetriever::CallCallbackOnError(
    webapps::InstallableStatusCode error_code) {
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
        .Run(/*manifest=*/nullptr,
             /*installable_check_passed_for_web_app=*/false,
             /*error_code=*/
             error_code);
  } else if (get_icons_callback_) {
    std::move(get_icons_callback_)
        .Run(IconsDownloadedResult::kPrimaryPageChanged, IconsMap{},
             DownloadedIconsHttpResults{});
  }
}

// TODO(b/302531937): Make this a utility that can be used through out the
// web_applications/ system.
bool WebAppDataRetriever::ShouldStopRetrieval() const {
  return !web_contents() || web_contents()->IsBeingDestroyed() ||
         web_contents()->GetBrowserContext()->ShutdownStarted();
}

}  // namespace web_app
