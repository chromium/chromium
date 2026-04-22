// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/drive_picker_host/drive_picker_host_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/drive_picker_host_resources.h"
#include "chrome/grit/drive_picker_host_resources_map.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

DrivePickerHostUIConfig::DrivePickerHostUIConfig()
    : DefaultTopChromeWebUIConfig(content::kChromeUIScheme,
                                  chrome::kChromeUIDrivePickerHostHost) {}

bool DrivePickerHostUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(
      omnibox::kComposeboxDriveContextMenuOption);
}

DrivePickerHostUI::DrivePickerHostUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIDrivePickerHostHost);

  webui::SetupWebUIDataSource(source, kDrivePickerHostResources,
                              IDR_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_HTML);
}

DrivePickerHostUI::~DrivePickerHostUI() = default;

void DrivePickerHostUI::BindInterface(
    mojo::PendingReceiver<drive_picker_host::mojom::DrivePickerHostHandler>
        receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(DrivePickerHostUI)
