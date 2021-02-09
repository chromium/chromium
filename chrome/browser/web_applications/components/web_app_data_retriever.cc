// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_data_retriever.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/webapps/common/web_page_metadata_agent.mojom.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {

WebAppDataRetriever::WebAppDataRetriever() = default;

WebAppDataRetriever::~WebAppDataRetriever() = default;

void WebAppDataRetriever::GetWebApplicationInfo(
    content::WebContents* web_contents,
    GetWebApplicationInfoCallback callback) {
  Observe(web_contents);

  // Concurrent calls are not allowed.
  DCHECK(!get_web_app_info_callback_);
  get_web_app_info_callback_ = std::move(callback);

  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (!entry) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&WebAppDataRetriever::CallCallbackOnError,
                                  weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // Makes a copy of WebContents fields right after Commit but before a mojo
  // request to the renderer process.
  default_web_application_info_ = std::make_unique<WebApplicationInfo>();
  default_web_application_info_->start_url =
      web_contents->GetLastCommittedURL();
  default_web_application_info_->title = web_contents->GetTitle();
  if (default_web_application_info_->title.empty()) {
    default_web_application_info_->title =
        base::UTF8ToUTF16(default_web_application_info_->start_url.spec());
  }

  mojo::AssociatedRemote<webapps::mojom::WebPageMetadataAgent> metadata_agent;
  web_contents->GetMainFrame()->GetRemoteAssociatedInterfaces()->GetInterface(
      &metadata_agent);

  // Set the error handler so that we can run |get_web_app_info_callback_| if
  // the WebContents or the RenderFrameHost are destroyed and the connection
  // to ChromeRenderFrame is lost.
  metadata_agent.set_disconnect_handler(
      base::BindOnce(&WebAppDataRetriever::CallCallbackOnError,
                     weak_ptr_factory_.GetWeakPtr()));
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
    CheckInstallabilityCallback callback) {
  webapps::InstallableManager* installable_manager =
      webapps::InstallableManager::FromWebContents(web_contents);
  DCHECK(installable_manager);

  Observe(web_contents);

  // Concurrent calls are not allowed.
  DCHECK(!check_installability_callback_);
  check_installability_callback_ = std::move(callback);

  // TODO(crbug.com/829232) Unify with other calls to GetData.
  webapps::InstallableParams params;
  params.check_eligibility = true;
  params.valid_primary_icon = true;
  params.valid_manifest = true;
  params.check_webapp_manifest_display = false;
  // Do not wait for a service worker if it doesn't exist.
  params.has_worker = !bypass_service_worker_check;
  // Do not wait_for_worker. OnDidPerformInstallableCheck is always invoked.
  installable_manager->GetData(
      params, base::BindOnce(&WebAppDataRetriever::OnDidPerformInstallableCheck,
                             weak_ptr_factory_.GetWeakPtr()));
}

void WebAppDataRetriever::GetIcons(content::WebContents* web_contents,
                                   const std::vector<GURL>& icon_urls,
                                   bool skip_page_favicons,
                                   WebAppIconDownloader::Histogram histogram,
                                   GetIconsCallback callback) {
  Observe(web_contents);

  // Concurrent calls are not allowed.
  CHECK(!get_icons_callback_);
  get_icons_callback_ = std::move(callback);

  // TODO(loyso): Refactor WebAppIconDownloader: crbug.com/907296.
  icon_downloader_ = std::make_unique<WebAppIconDownloader>(
      web_contents, icon_urls, histogram,
      base::BindOnce(&WebAppDataRetriever::OnIconsDownloaded,
                     weak_ptr_factory_.GetWeakPtr()));

  if (skip_page_favicons)
    icon_downloader_->SkipPageFavicons();

  icon_downloader_->Start();
}

void WebAppDataRetriever::WebContentsDestroyed() {
  CallCallbackOnError();
}

void WebAppDataRetriever::RenderProcessGone(base::TerminationStatus status) {
  CallCallbackOnError();
}

void WebAppDataRetriever::OnGetWebPageMetadata(
    mojo::AssociatedRemote<webapps::mojom::WebPageMetadataAgent> metadata_agent,
    int last_committed_nav_entry_unique_id,
    webapps::mojom::WebPageMetadataPtr web_page_metadata) {
  if (ShouldStopRetrieval())
    return;

  DCHECK(default_web_application_info_);

  content::WebContents* contents = web_contents();
  Observe(nullptr);

  std::unique_ptr<WebApplicationInfo> info;

  content::NavigationEntry* entry =
      contents->GetController().GetLastCommittedEntry();

  if (entry) {
    if (entry->GetUniqueID() == last_committed_nav_entry_unique_id) {
      info = std::make_unique<WebApplicationInfo>(*web_page_metadata);
      if (info->start_url.is_empty())
        info->start_url = std::move(default_web_application_info_->start_url);
      if (info->title.empty())
        info->title = std::move(default_web_application_info_->title);
    } else {
      // WebContents navigation state changed during the call. Ignore the mojo
      // request result. Use default initial info instead.
      info = std::move(default_web_application_info_);
    }
  }

  default_web_application_info_.reset();

  std::move(get_web_app_info_callback_).Run(std::move(info));
}

void WebAppDataRetriever::OnDidPerformInstallableCheck(
    const webapps::InstallableData& data) {
  if (ShouldStopRetrieval())
    return;

  Observe(nullptr);

  DCHECK(data.manifest_url.is_valid() || data.manifest.IsEmpty());

  const bool is_installable = data.NoBlockingErrors();
  DCHECK(!is_installable || data.valid_manifest);
  base::Optional<blink::Manifest> opt_manifest;
  if (!data.manifest.IsEmpty())
    opt_manifest = data.manifest;

  std::move(check_installability_callback_)
      .Run(std::move(opt_manifest), data.manifest_url, data.valid_manifest,
           is_installable);
}

void WebAppDataRetriever::OnIconsDownloaded(bool success, IconsMap icons_map) {
  if (ShouldStopRetrieval())
    return;

  Observe(nullptr);
  icon_downloader_.reset();
  std::move(get_icons_callback_).Run(std::move(icons_map));
}

void WebAppDataRetriever::CallCallbackOnError() {
  Observe(nullptr);
  DCHECK(ShouldStopRetrieval());

  default_web_application_info_.reset();

  // Call a callback as a tail call. The callback may destroy |this|.
  if (get_web_app_info_callback_) {
    std::move(get_web_app_info_callback_).Run(nullptr);
  } else if (check_installability_callback_) {
    std::move(check_installability_callback_)
        .Run(/*manifest=*/base::nullopt, /*manifest_url=*/GURL(),
             /*valid_manifest_for_web_app=*/false,
             /*is_installable=*/false);
  } else if (get_icons_callback_) {
    std::move(get_icons_callback_).Run(IconsMap{});
  }
}

bool WebAppDataRetriever::ShouldStopRetrieval() const {
  return !web_contents();
}

}  // namespace web_app
