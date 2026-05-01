// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/drive_picker_host/untrusted/drive_picker_host_untrusted_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/drive_picker_host_untrusted_resources.h"
#include "chrome/grit/drive_picker_host_untrusted_resources_map.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
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
      source, kDrivePickerHostUntrustedResources,
      IDR_DRIVE_PICKER_HOST_UNTRUSTED_DRIVE_PICKER_HOST_UNTRUSTED_HTML);
}

DrivePickerUntrustedHostUI::~DrivePickerUntrustedHostUI() = default;

void DrivePickerUntrustedHostUI::BindInterface(
    mojo::PendingReceiver<
        drive_picker_host_untrusted::mojom::DrivePickerUntrustedHostHandler>
        receiver) {
  untrusted_host_receiver_.reset();
  untrusted_host_receiver_.Bind(std::move(receiver));
}

void DrivePickerUntrustedHostUI::BindInterface(
    mojo::PendingReceiver<drive_picker_host_untrusted::mojom::DrivePickerBridge>
        receiver) {
  bridge_receiver_.reset();
  bridge_receiver_.Bind(std::move(receiver));
}

void DrivePickerUntrustedHostUI::BindPage(
    mojo::PendingRemote<drive_picker_host_untrusted::mojom::Page> page) {
  page_.reset();
  page_.Bind(std::move(page));

  if (pending_request_) {
    page_->ShowDrivePicker(std::move(pending_request_));
  }
}

void DrivePickerUntrustedHostUI::ShowDrivePicker(
    mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>
        result_handler) {
  if (page_.is_bound() && page_.is_connected()) {
    page_->ShowDrivePicker(std::move(result_handler));
  } else {
    // Only the most recent request is kept if the page is not yet ready or
    // has been disconnected.
    pending_request_ = std::move(result_handler);
  }
}
