// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/drive_picker_host/untrusted/drive_picker_host_untrusted_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/drive_picker_host_untrusted_resources.h"
#include "chrome/grit/drive_picker_host_untrusted_resources_map.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/webui/webui_util.h"

// DrivePickerUntrustedHostUIConfig
DrivePickerUntrustedHostUIConfig::DrivePickerUntrustedHostUIConfig()
    : DefaultWebUIConfig(content::kChromeUIUntrustedScheme,
                         chrome::kChromeUIDrivePickerHostHost) {}

DrivePickerUntrustedHostUIConfig::~DrivePickerUntrustedHostUIConfig() = default;

bool DrivePickerUntrustedHostUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(
      omnibox::kComposeboxDriveContextMenuOption);
}

// DrivePickerUntrustedHostUI
WEB_UI_CONTROLLER_TYPE_IMPL(DrivePickerUntrustedHostUI)

DrivePickerUntrustedHostUI::DrivePickerUntrustedHostUI(content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIDrivePickerHostUntrustedURL);

  webui::SetupWebUIDataSource(
      source,kDrivePickerHostUntrustedResources,
      IDR_DRIVE_PICKER_HOST_UNTRUSTED_DRIVE_PICKER_HOST_UNTRUSTED_HTML);

  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);
}

DrivePickerUntrustedHostUI::~DrivePickerUntrustedHostUI() = default;

void DrivePickerUntrustedHostUI::BindInterface(
    mojo::PendingReceiver<
        drive_picker_host_untrusted::mojom::DrivePickerUntrustedHostHandler>
        receiver) {
  handler_receiver_.reset();
  handler_receiver_.Bind(std::move(receiver));
}
