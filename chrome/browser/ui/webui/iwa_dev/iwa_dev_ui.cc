// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/iwa_dev/iwa_dev_ui.h"

#include "base/check_is_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/iwa_dev/iwa_dev_page_handler.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/iwa_dev_resources.h"
#include "chrome/grit/iwa_dev_resources_map.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/isolated_web_apps_policy.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/webui/webui_util.h"

bool IwaDevUIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  return content::AreIsolatedWebAppsEnabled(browser_context) &&
         base::FeatureList::IsEnabled(features::kIsolatedWebAppDevUi);
}

IwaDevUI::IwaDevUI(content::WebUI* web_ui) : ui::MojoWebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIIwaDevHost);
  webui::SetupWebUIDataSource(source, kIwaDevResources,
                              IDR_IWA_DEV_IWA_DEV_HTML);
  source->AddBoolean("isIwaDevModeEnabled",
                     web_app::IsIwaDevModeEnabled(profile));
}

IwaDevUI::~IwaDevUI() = default;

void IwaDevUI::BindInterface(
    mojo::PendingReceiver<iwa_dev::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void IwaDevUI::CreatePageHandler(
    mojo::PendingRemote<iwa_dev::mojom::Page> page,
    mojo::PendingReceiver<iwa_dev::mojom::PageHandler> receiver) {
  if (!web_app::IsIwaDevModeEnabled(Profile::FromWebUI(web_ui()))) {
    return;
  }
  page_handler_ = std::make_unique<IwaDevPageHandler>(web_ui(), std::move(page),
                                                      std::move(receiver));
}

IwaDevPageHandler* IwaDevUI::GetHandlerForTesting() {
  CHECK_IS_TEST();
  return page_handler_.get();
}

WEB_UI_CONTROLLER_TYPE_IMPL(IwaDevUI)

// static
scoped_refptr<base::RefCountedMemory> IwaDevUI::GetFaviconResourceBytes(
    ui::ResourceScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_IWA_DEV_ICON, scale_factor);
}
