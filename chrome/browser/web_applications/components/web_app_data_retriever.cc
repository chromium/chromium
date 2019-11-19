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
#include "chrome/browser/installable/installable_data.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/web_application_info.h"
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

  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame;
  web_contents->GetMainFrame()->GetRemoteAssociatedInterfaces()->GetInterface(
      &chrome_render_frame);

  // Set the error handler so that we can run |get_web_app_info_callback_| if
  // the WebContents or the RenderFrameHost are destroyed and the connection
  // to ChromeRenderFrame is lost.
  chrome_render_frame.set_disconnect_handler(
      base::BindOnce(&WebAppDataRetriever::CallCallbackOnError,
                     weak_ptr_factory_.GetWeakPtr()));
  // Bind the InterfacePtr into the callback so that it's kept alive
  // until there's either a connection error or a response.
  auto* web_app_info_proxy = chrome_render_frame.get();
  web_app_info_proxy->GetWebApplicationInfo(
      base::BindOnce(&WebAppDataRetriever::OnGetWebApplicationInfo,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(chrome_render_frame), entry->GetUniqueID()));
}

void WebAppDataRetriever::CheckInstallabilityAndRetrieveManifest(
    content::WebContents* web_contents,
    bool bypass_service_worker_check,
    CheckInstallabilityCallback callback) {
  InstallableManager* installable_manager =
      InstallableManager::FromWebContents(web_contents);
  DCHECK(installable_manager);

  Observe(web_contents);

  // Concurrent calls are not allowed.
  DCHECK(!check_installability_callback_);
  check_installability_callback_ = std::move(callback);

  // TODO(crbug.com/829232) Unify with other calls to GetData.
  InstallableParams params;
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

void WebAppDataRetriever::OnGetWebApplicationInfo(
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
        chrome_render_frame,
    int last_committed_nav_entry_unique_id,
    const WebApplicationInfo& web_app_info) {
  if (ShouldStopRetrieval())
    return;

  content::WebContents* contents = web_contents();
  Observe(nullptr);

  content::NavigationEntry* entry =
      contents->GetController().GetLastCommittedEntry();
  if (!entry || last_committed_nav_entry_unique_id != entry->GetUniqueID()) {
    std::move(get_web_app_info_callback_).Run(nullptr);
    return;
  }

  auto info = std::make_unique<WebApplicationInfo>(web_app_info);
  if (info->app_url.is_empty())
    info->app_url = contents->GetLastCommittedURL();

  if (info->title.empty())
    info->title = contents->GetTitle();
  if (info->title.empty())
    info->title = base::UTF8ToUTF16(info->app_url.spec());

  std::move(get_web_app_info_callback_).Run(std::move(info));
}

void WebAppDataRetriever::OnDidPerformInstallableCheck(
    const InstallableData& data) {
  if (ShouldStopRetrieval())
    return;

  Observe(nullptr);

  DCHECK(data.manifest);
  DCHECK(data.manifest_url.is_valid() || data.manifest->IsEmpty());

  const bool is_installable = data.errors.empty();
  DCHECK(!is_installable || data.valid_manifest);
  DCHECK(!data.valid_manifest || !data.manifest->IsEmpty());

  std::move(check_installability_callback_)
      .Run(*data.manifest, data.valid_manifest, is_installable);
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

  // Call a callback as a tail call. The callback may destroy |this|.
  if (get_web_app_info_callback_) {
    std::move(get_web_app_info_callback_).Run(nullptr);
  } else if (check_installability_callback_) {
    std::move(check_installability_callback_)
        .Run(base::nullopt, /*valid_manifest_for_web_app=*/false,
             /*is_installable=*/false);
  } else if (get_icons_callback_) {
    std::move(get_icons_callback_).Run(IconsMap{});
  }
}

bool WebAppDataRetriever::ShouldStopRetrieval() const {
  return !web_contents();
}

}  // namespace web_app
