// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/personalization_app/personalization_app_ui.h"

#include "base/strings/strcat.h"
#include "chromeos/components/personalization_app/personalization_app_ui_delegate.h"
#include "chromeos/components/personalization_app/personalization_app_url_constants.h"
#include "chromeos/grit/chromeos_personalization_app_resources.h"
#include "chromeos/grit/chromeos_personalization_app_resources_map.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom-shared.h"
#include "ui/resources/grit/webui_generated_resources.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace chromeos {

namespace {

bool ShouldIncludeResource(const webui::ResourcePath& resource) {
  return base::StartsWith(resource.path, "trusted") ||
         base::StartsWith(resource.path, "common") ||
         resource.id == IDR_CHROMEOS_PERSONALIZATION_APP_ICON_192_PNG;
}

void AddResources(content::WebUIDataSource* source) {
  source->AddResourcePath("",
                          IDR_CHROMEOS_PERSONALIZATION_APP_TRUSTED_INDEX_HTML);

  const auto resources =
      base::make_span(kChromeosPersonalizationAppResources,
                      kChromeosPersonalizationAppResourcesSize);

  for (const auto& resource : resources) {
    if (ShouldIncludeResource(resource))
      source->AddResourcePath(resource.path, resource.id);
  }
  // Mirror assert.m.js here so that it is accessible at the same path in
  // trusted and untrusted context.
  source->AddResourcePath("assert.m.js", IDR_WEBUI_JS_ASSERT_M_JS);

  source->AddResourcePath("test_loader.html", IDR_WEBUI_HTML_TEST_LOADER_HTML);
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
  source->AddResourcePath("test_loader_util.js",
                          IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);

#if !DCHECK_IS_ON()
  // Add a default path to avoid crash when not debugging.
  source->SetDefaultResource(
      IDR_CHROMEOS_PERSONALIZATION_APP_TRUSTED_INDEX_HTML);
#endif  // !DCHECK_IS_ON()
}

void AddStrings(content::WebUIDataSource* source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"title", IDS_PERSONALIZATION_APP_TITLE}};
  source->AddLocalizedStrings(kLocalizedStrings);
  source->UseStringsJs();
}

}  // namespace

PersonalizationAppUI::PersonalizationAppUI(
    content::WebUI* web_ui,
    std::unique_ptr<PersonalizationAppUiDelegate> delegate)
    : ui::MojoWebUIController(web_ui), delegate_(std::move(delegate)) {
  DCHECK(delegate_);

  std::unique_ptr<content::WebUIDataSource> source = base::WrapUnique(
      content::WebUIDataSource::Create(kChromeUIPersonalizationAppHost));

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://test 'self';");

  // Allow requesting a chrome-untrusted://personalization/ iframe.
  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc,
      base::StrCat(
          {"frame-src ", kChromeUIUntrustedPersonalizationAppURL, ";"}));

  // TODO(crbug/1169829) set up trusted types properly to allow Polymer to write
  // html
  source->DisableTrustedTypesCSP();

  AddResources(source.get());
  AddStrings(source.get());

  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, source.release());
}

PersonalizationAppUI::~PersonalizationAppUI() = default;

void PersonalizationAppUI::BindInterface(
    mojo::PendingReceiver<
        chromeos::personalization_app::mojom::WallpaperProvider> receiver) {
  delegate_->BindInterface(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(PersonalizationAppUI)

}  // namespace chromeos
