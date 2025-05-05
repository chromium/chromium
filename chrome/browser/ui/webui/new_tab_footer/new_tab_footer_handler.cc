// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_handler.h"

#include <utility>

#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "net/base/url_util.h"

NewTabFooterHandler::NewTabFooterHandler(
    mojo::PendingReceiver<new_tab_footer::mojom::NewTabFooterHandler>
        pending_handler,
    mojo::PendingRemote<new_tab_footer::mojom::NewTabFooterDocument>
        pending_document,
    content::WebContents* web_contents)
    : profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      web_contents_(web_contents),
      document_(std::move(pending_document)),
      handler_{this, std::move(pending_handler)} {}

NewTabFooterHandler::~NewTabFooterHandler() = default;

void NewTabFooterHandler::GetNtpExtensionAttribution(
    GetNtpExtensionAttributionCallback callback) {
  const extensions::Extension* ntp_extension =
      extensions::GetExtensionOverridingNewTabPage(profile_);
  if (!ntp_extension) {
    std::move(callback).Run(nullptr);
    return;
  }

  auto attribution = new_tab_footer::mojom::ExtensionAttribution::New();
  attribution->url = net::AppendOrReplaceQueryParameter(
      GURL(chrome::kChromeUIExtensionsURL), "id", ntp_extension->id());
  ;
  attribution->name = ntp_extension->name();
  std::move(callback).Run(std::move(attribution));
}
