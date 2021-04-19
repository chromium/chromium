// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/personalization_app/untrusted_personalization_app_ui_config.h"

#include "ash/constants/ash_features.h"
#include "base/strings/string_util.h"
#include "chromeos/components/personalization_app/personalization_app_url_constants.h"
#include "chromeos/grit/chromeos_personalization_app_resources.h"
#include "chromeos/grit/chromeos_personalization_app_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom-shared.h"
#include "ui/resources/grit/webui_generated_resources.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

class UntrustedPersonalizationAppUI : public ui::UntrustedWebUIController {
 public:
  explicit UntrustedPersonalizationAppUI(content::WebUI* web_ui)
      : ui::UntrustedWebUIController(web_ui) {
    std::unique_ptr<content::WebUIDataSource> source =
        base::WrapUnique(content::WebUIDataSource::Create(
            kChromeUIUntrustedPersonalizationAppURL));

    const auto resources =
        base::make_span(kChromeosPersonalizationAppResources,
                        kChromeosPersonalizationAppResourcesSize);

    for (const auto& resource : resources) {
      if (base::StartsWith(resource.path, "untrusted") ||
          base::StartsWith(resource.path, "common"))
        source->AddResourcePath(resource.path, resource.id);
    }
    // Mirror assert.m.js here so that it is accessible at the same path in
    // trusted and untrusted context.
    source->AddResourcePath("assert.m.js", IDR_WEBUI_JS_ASSERT_M_JS);

    source->AddFrameAncestor(GURL(kChromeUIPersonalizationAppURL));

    // Allow images only from this url.
    source->OverrideContentSecurityPolicy(
        network::mojom::CSPDirectiveName::ImgSrc,
        "img-src https://*.googleusercontent.com;");

    source->OverrideContentSecurityPolicy(
        network::mojom::CSPDirectiveName::ScriptSrc, "script-src 'self';");

#if !DCHECK_IS_ON()
    // When DCHECKs are off and a user goes to an invalid url serve a default
    // page to avoid crashing. We crash when DCHECKs are on to make it clearer
    // that a resource path was not property specified.
    source->SetDefaultResource(
        IDR_CHROMEOS_PERSONALIZATION_APP_UNTRUSTED_COLLECTIONS_HTML);
#endif  // !DCHECK_IS_ON()

    auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
    content::WebUIDataSource::Add(browser_context, source.release());
  }

  UntrustedPersonalizationAppUI(const UntrustedPersonalizationAppUI&) = delete;
  UntrustedPersonalizationAppUI& operator=(
      const UntrustedPersonalizationAppUI&) = delete;
  ~UntrustedPersonalizationAppUI() override = default;
};

}  // namespace

UntrustedPersonalizationAppUIConfig::UntrustedPersonalizationAppUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  kChromeUIPersonalizationAppHost) {}

UntrustedPersonalizationAppUIConfig::~UntrustedPersonalizationAppUIConfig() =
    default;

bool UntrustedPersonalizationAppUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return ash::features::IsWallpaperWebUIEnabled() &&
         !browser_context->IsOffTheRecord();
}

std::unique_ptr<content::WebUIController>
UntrustedPersonalizationAppUIConfig::CreateWebUIController(
    content::WebUI* web_ui) {
  return std::make_unique<UntrustedPersonalizationAppUI>(web_ui);
}

}  // namespace chromeos
