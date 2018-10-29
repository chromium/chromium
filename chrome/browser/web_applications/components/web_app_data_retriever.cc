// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_data_retriever.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
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
  // Concurrent calls are not allowed.
  CHECK(!get_web_app_info_callback_);
  get_web_app_info_callback_ = std::move(callback);

  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (!entry) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(get_web_app_info_callback_), nullptr));
    return;
  }

  chrome::mojom::ChromeRenderFrameAssociatedPtr chrome_render_frame;
  web_contents->GetMainFrame()->GetRemoteAssociatedInterfaces()->GetInterface(
      &chrome_render_frame);

  // Set the error handler so that we can run |get_web_app_info_callback_| if
  // the WebContents or the RenderFrameHost are destroyed and the connection
  // to ChromeRenderFrame is lost.
  chrome_render_frame.set_connection_error_handler(
      base::BindOnce(&WebAppDataRetriever::OnGetWebApplicationInfoFailed,
                     weak_ptr_factory_.GetWeakPtr()));
  // Bind the InterfacePtr into the callback so that it's kept alive
  // until there's either a connection error or a response.
  auto* web_app_info_proxy = chrome_render_frame.get();
  web_app_info_proxy->GetWebApplicationInfo(base::BindOnce(
      &WebAppDataRetriever::OnGetWebApplicationInfo,
      weak_ptr_factory_.GetWeakPtr(), std::move(chrome_render_frame),
      web_contents, entry->GetUniqueID()));
}

void WebAppDataRetriever::GetIcons(const GURL& app_url,
                                   const std::vector<GURL>& icon_urls,
                                   GetIconsCallback callback) {
  // TODO(crbug.com/864904): Download icons using |icon_urls|.

  // Generate missing icons.
  static constexpr int kIconSizesToGenerate[] = {
      icon_size::k32,     icon_size::k32 * 2, icon_size::k48,
      icon_size::k48 * 2, icon_size::k128,    icon_size::k128 * 2,
  };

  // Get the letter to use in the generated icon.
  char icon_letter = ' ';
  std::string domain_and_registry(
      net::registry_controlled_domains::GetDomainAndRegistry(
          app_url,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));

  // TODO(crbug.com/867311): Decode the app URL or the domain before retrieving
  // the first character, otherwise we generate an icon with "x" if the domain
  // or app URL starts with a UTF-8 character.
  if (!domain_and_registry.empty()) {
    icon_letter = domain_and_registry[0];
  } else if (app_url.has_host()) {
    icon_letter = app_url.host_piece()[0];
  }
  DCHECK(icon_letter >= '!' && icon_letter <= '~');

  std::vector<WebApplicationInfo::IconInfo> icons;
  for (int size : kIconSizesToGenerate) {
    WebApplicationInfo::IconInfo icon_info;
    icon_info.width = size;
    icon_info.height = size;
    icon_info.data = GenerateBitmap(size, SK_ColorDKGRAY, icon_letter);
    icons.push_back(icon_info);
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(icons)));
}

void WebAppDataRetriever::OnGetWebApplicationInfo(
    chrome::mojom::ChromeRenderFrameAssociatedPtr chrome_render_frame,
    content::WebContents* web_contents,
    int last_committed_nav_entry_unique_id,
    const WebApplicationInfo& web_app_info) {
  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (!entry || last_committed_nav_entry_unique_id != entry->GetUniqueID()) {
    std::move(get_web_app_info_callback_).Run(nullptr);
    return;
  }

  auto info = std::make_unique<WebApplicationInfo>(web_app_info);
  if (info->app_url.is_empty())
    info->app_url = web_contents->GetLastCommittedURL();

  if (info->title.empty())
    info->title = web_contents->GetTitle();
  if (info->title.empty())
    info->title = base::UTF8ToUTF16(info->app_url.spec());

  std::move(get_web_app_info_callback_).Run(std::move(info));
}

void WebAppDataRetriever::OnGetWebApplicationInfoFailed() {
  std::move(get_web_app_info_callback_).Run(nullptr);
}

}  // namespace web_app
