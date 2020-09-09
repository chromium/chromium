// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/media_app_ui/media_app_ui.h"

#include <utility>

#include "chromeos/components/media_app_ui/media_app_guest_ui.h"
#include "chromeos/components/media_app_ui/media_app_page_handler.h"
#include "chromeos/components/media_app_ui/url_constants.h"
#include "chromeos/components/web_applications/manifest_request_filter.h"
#include "chromeos/grit/chromeos_media_app_bundle_resources.h"
#include "chromeos/grit/chromeos_media_app_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/webui/webui_allowlist.h"

namespace chromeos {
namespace {

content::WebUIDataSource* CreateHostDataSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(kChromeUIMediaAppHost);

  // Add resources from chromeos_media_app_resources.pak.
  source->SetDefaultResource(IDR_MEDIA_APP_INDEX_HTML);
  source->AddResourcePath("mojo_api_bootstrap.js",
                          IDR_MEDIA_APP_MOJO_API_BOOTSTRAP_JS);
  source->AddResourcePath("media_app.mojom-lite.js",
                          IDR_MEDIA_APP_MEDIA_APP_MOJOM_JS);
  source->AddResourcePath("media_app_index_scripts.js",
                          IDR_MEDIA_APP_INDEX_SCRIPTS_JS);
  source->AddLocalizedString("appTitle", IDS_MEDIA_APP_APP_NAME);

  // Redirects "system_assets/app_icon_*.png" (from manifest.json) to the icons
  // for the gallery app.
  // TODO(b/141588875): Switch these to IDR_MEDIA_APP_APP_ICON_*_PNG in the
  // internal media_app_bundle_resources.grd file (and add more icon
  // resolutions) when the final icon is ready.
  source->AddResourcePath("system_assets/app_icon_16.png",
                          IDR_MEDIA_APP_GALLERY_ICON_16_PNG);
  source->AddResourcePath("system_assets/app_icon_32.png",
                          IDR_MEDIA_APP_GALLERY_ICON_32_PNG);
  source->AddResourcePath("system_assets/app_icon_48.png",
                          IDR_MEDIA_APP_GALLERY_ICON_48_PNG);
  source->AddResourcePath("system_assets/app_icon_64.png",
                          IDR_MEDIA_APP_GALLERY_ICON_64_PNG);
  source->AddResourcePath("system_assets/app_icon_96.png",
                          IDR_MEDIA_APP_GALLERY_ICON_96_PNG);
  source->AddResourcePath("system_assets/app_icon_128.png",
                          IDR_MEDIA_APP_GALLERY_ICON_128_PNG);
  source->AddResourcePath("system_assets/app_icon_192.png",
                          IDR_MEDIA_APP_GALLERY_ICON_192_PNG);
  source->AddResourcePath("system_assets/app_icon_256.png",
                          IDR_MEDIA_APP_GALLERY_ICON_256_PNG);
  return source;
}

}  // namespace

MediaAppUI::MediaAppUI(content::WebUI* web_ui,
                       std::unique_ptr<MediaAppUIDelegate> delegate)
    : MojoWebUIController(web_ui), delegate_(std::move(delegate)) {
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource* host_source = CreateHostDataSource();
  content::WebUIDataSource::Add(browser_context, host_source);

  // The guest is in an <iframe>. Add it to CSP.
  std::string csp = std::string("frame-src ") + kChromeUIMediaAppGuestURL + ";";
  host_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, csp);

  // Register auto-granted permissions.
  auto* allowlist = WebUIAllowlist::GetOrCreate(browser_context);
  const url::Origin host_origin =
      url::Origin::Create(GURL(kChromeUIMediaAppURL));
  allowlist->RegisterAutoGrantedPermissions(
      host_origin, {
                       ContentSettingsType::COOKIES,
                       ContentSettingsType::FILE_SYSTEM_READ_GUARD,
                       ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                       ContentSettingsType::IMAGES,
                       ContentSettingsType::JAVASCRIPT,
                       ContentSettingsType::SOUND,
                   });

  content::WebUIDataSource* untrusted_source =
      CreateMediaAppUntrustedDataSource(delegate_.get());
  content::WebUIDataSource::Add(browser_context, untrusted_source);

  // Add ability to request chrome-untrusted: URLs.
  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);
}

MediaAppUI::~MediaAppUI() = default;

void MediaAppUI::BindInterface(
    mojo::PendingReceiver<media_app_ui::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void MediaAppUI::CreatePageHandler(
    mojo::PendingReceiver<media_app_ui::mojom::PageHandler> receiver) {
  page_handler_ =
      std::make_unique<MediaAppPageHandler>(this, std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(MediaAppUI)

}  // namespace chromeos
