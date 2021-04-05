// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/personalization_app/personalization_app_ui.h"

#include "chromeos/components/personalization_app/personalization_app_ui_delegate.h"
#include "chromeos/components/personalization_app/personalization_app_url_constants.h"
#include "chromeos/grit/chromeos_personalization_app_resources.h"
#include "chromeos/grit/chromeos_personalization_app_resources_map.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/resources/grit/webui_generated_resources.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace chromeos {

namespace {

void AddResources(content::WebUIDataSource* source) {
  source->AddResourcePath("", IDR_CHROMEOS_PERSONALIZATION_APP_INDEX_HTML);
  source->AddResourcePaths(
      base::make_span(kChromeosPersonalizationAppResources,
                      kChromeosPersonalizationAppResourcesSize));

  source->AddResourcePath("test_loader.html", IDR_WEBUI_HTML_TEST_LOADER_HTML);
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
  source->AddResourcePath("test_loader_util.js",
                          IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);

#if !DCHECK_IS_ON()
  source->SetDefaultResource(IDR_CHROMEOS_PERSONALIZATION_APP_INDEX_HTML);
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
