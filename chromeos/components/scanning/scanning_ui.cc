// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/scanning/scanning_ui.h"

#include <string>

#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "chromeos/components/scanning/mojom/scanning.mojom.h"
#include "chromeos/components/scanning/url_constants.h"
#include "chromeos/grit/chromeos_scanning_app_resources.h"
#include "chromeos/grit/chromeos_scanning_app_resources_map.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_resources.h"

namespace chromeos {

namespace {

constexpr char kGeneratedPath[] =
    "@out_folder@/gen/chromeos/components/scanning/resources/";

// TODO(jschettler): Replace with webui::SetUpWebUIDataSource() once it no
// longer requires a dependency on //chrome/browser.
void SetUpWebUIDataSource(content::WebUIDataSource* source,
                          base::span<const GritResourceMap> resources,
                          const std::string& generated_path,
                          int default_resource) {
  for (const auto& resource : resources) {
    std::string path = resource.name;
    if (path.rfind(generated_path, 0) == 0)
      path = path.substr(generated_path.size());

    source->AddResourcePath(path, resource.value);
  }

  source->SetDefaultResource(default_resource);
  source->AddResourcePath("test_loader.html", IDR_WEBUI_HTML_TEST_LOADER);
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER);
}

void AddScanningAppStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"appTitle", IDS_SCANNING_APP_TITLE},
      {"scannerDropdownLabel", IDS_SCANNING_APP_SCANNER_DROPDOWN_LABEL},
      {"noScannersText", IDS_SCANNING_APP_NO_SCANNERS_TEXT},
      {"sourceDropdownLabel", IDS_SCANNING_APP_SOURCE_DROPDOWN_LABEL}};

  for (const auto& str : kLocalizedStrings)
    html_source->AddLocalizedString(str.name, str.id);

  html_source->UseStringsJs();
}

}  // namespace

ScanningUI::ScanningUI(content::WebUI* web_ui, BindScanServiceCallback callback)
    : ui::MojoWebUIController(web_ui),
      bind_pending_receiver_callback_(std::move(callback)) {
  auto html_source = base::WrapUnique(
      content::WebUIDataSource::Create(kChromeUIScanningAppHost));
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://test 'self';");
  html_source->DisableTrustedTypesCSP();

  const auto resources = base::make_span(kChromeosScanningAppResources,
                                         kChromeosScanningAppResourcesSize);
  SetUpWebUIDataSource(html_source.get(), resources, kGeneratedPath,
                       IDR_SCANNING_APP_INDEX_HTML);

  html_source->AddResourcePath("scanning.mojom-lite.js",
                               IDR_SCANNING_MOJO_LITE_JS);

  AddScanningAppStrings(html_source.get());

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                html_source.release());
}

ScanningUI::~ScanningUI() = default;

void ScanningUI::BindInterface(
    mojo::PendingReceiver<scanning::mojom::ScanService> pending_receiver) {
  bind_pending_receiver_callback_.Run(std::move(pending_receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(ScanningUI)

}  // namespace chromeos
