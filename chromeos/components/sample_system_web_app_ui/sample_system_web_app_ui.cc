// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sample_system_web_app_ui/sample_system_web_app_ui.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "chromeos/components/sample_system_web_app_ui/url_constants.h"
#include "chromeos/grit/chromeos_sample_system_web_app_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/webui/webui_allowlist.h"

namespace chromeos {
namespace {
content::WebUIDataSource* CreateUntrustedSampleSystemWebAppDataSource() {
  content::WebUIDataSource* untrusted_source =
      content::WebUIDataSource::Create(kChromeUIUntrustedSampleSystemWebAppURL);
  untrusted_source->AddResourcePath("untrusted.html",
                                    IDR_SAMPLE_SYSTEM_WEB_APP_UNTRUSTED_HTML);
  untrusted_source->AddResourcePath("untrusted.js",
                                    IDR_SAMPLE_SYSTEM_WEB_APP_UNTRUSTED_JS);
  untrusted_source->AddFrameAncestor(GURL(kChromeUISampleSystemWebAppURL));
  return untrusted_source;
}
}  // namespace

SampleSystemWebAppUI::SampleSystemWebAppUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  auto trusted_source = base::WrapUnique(
      content::WebUIDataSource::Create(kChromeUISampleSystemWebAppHost));

  trusted_source->AddResourcePath("", IDR_SAMPLE_SYSTEM_WEB_APP_INDEX_HTML);
  trusted_source->AddResourcePath("sandbox.html",
                                  IDR_SAMPLE_SYSTEM_WEB_APP_SANDBOX_HTML);
  trusted_source->AddResourcePath("app_icon_192.png",
                                  IDR_SAMPLE_SYSTEM_WEB_APP_ICON_192);

#if !DCHECK_IS_ON()
  // If a user goes to an invalid url and non-DCHECK mode (DHECK = debug mode)
  // is set, serve a default page so the user sees your default page instead
  // of an unexpected error. But if DCHECK is set, the user will be a
  // developer and be able to identify an error occurred.
  trusted_source->SetDefaultResource(IDR_SAMPLE_SYSTEM_WEB_APP_INDEX_HTML);
#endif  // !DCHECK_IS_ON()

  // We need a CSP override to use the chrome-untrusted:// scheme in the host.
  std::string csp =
      std::string("frame-src ") + kChromeUIUntrustedSampleSystemWebAppURL + ";";
  trusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, csp);
  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, trusted_source.release());
  content::WebUIDataSource::Add(browser_context,
                                CreateUntrustedSampleSystemWebAppDataSource());

  // Add ability to request chrome-untrusted: URLs
  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);

  // Register common permissions for chrome-untrusted:// pages.
  // TODO(https://crbug.com/1113568): Remove this after common permissions are
  // granted by default.
  auto* webui_allowlist = WebUIAllowlist::GetOrCreate(browser_context);
  const url::Origin untrusted_sample_system_web_app_origin =
      url::Origin::Create(GURL(kChromeUIUntrustedSampleSystemWebAppURL));
  for (const auto& permission : {
           ContentSettingsType::COOKIES,
           ContentSettingsType::JAVASCRIPT,
           ContentSettingsType::IMAGES,
           ContentSettingsType::SOUND,
       }) {
    webui_allowlist->RegisterAutoGrantedPermission(
        untrusted_sample_system_web_app_origin, permission);
  }
}

SampleSystemWebAppUI::~SampleSystemWebAppUI() = default;

}  // namespace chromeos
