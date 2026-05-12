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
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/webui/webui_util.h"
#include "url/gurl.h"

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

DrivePickerUntrustedHostUI::PendingRequest::PendingRequest(
    mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>
        handler,
    drive_picker_host_untrusted::mojom::DrivePickerKeysPtr keys)
    : result_handler(std::move(handler)), keys(std::move(keys)) {}

DrivePickerUntrustedHostUI::PendingRequest::~PendingRequest() = default;

DrivePickerUntrustedHostUI::DrivePickerUntrustedHostUI(content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIDrivePickerHostUntrustedURL);

  webui::SetupWebUIDataSource(
      source, kDrivePickerHostUntrustedResources,
      IDR_DRIVE_PICKER_HOST_UNTRUSTED_DRIVE_PICKER_HOST_UNTRUSTED_HTML);

  source->AddFrameAncestor(GURL(chrome::kChromeUIDrivePickerHostURL));

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src 'self' chrome-untrusted://resources/ "
      "https://apis.google.com;");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ConnectSrc,
      "connect-src 'self' https://apis.google.com;");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc,
      "frame-src 'self' https://docs.google.com;");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc,
      "img-src 'self' chrome-untrusted://resources/ "
      "https://*.googleusercontent.com https://*.google.com data:;");

  // This is required to allow the Google Picker API to be loaded via GAPI.
  // Otherwise, the picker will not be able to load due to CSP restrictions
  // since it requires a TrustedTypePolicy to be set up.
  source->DisableTrustedTypesCSP();
}

DrivePickerUntrustedHostUI::~DrivePickerUntrustedHostUI() = default;

void DrivePickerUntrustedHostUI::BindInterface(
    mojo::PendingReceiver<
        drive_picker_host_untrusted::mojom::PageHandlerFactory> receiver) {
  factory_receiver_.reset();
  factory_receiver_.Bind(std::move(receiver));
}

void DrivePickerUntrustedHostUI::BindInterface(
    mojo::PendingReceiver<drive_picker_host_untrusted::mojom::DrivePickerBridge>
        receiver) {
  bridge_receiver_.reset();
  bridge_receiver_.Bind(std::move(receiver));
}

void DrivePickerUntrustedHostUI::CreatePageHandler(
    mojo::PendingRemote<drive_picker_host_untrusted::mojom::Page> page,
    mojo::PendingReceiver<drive_picker_host_untrusted::mojom::PageHandler>
        handler) {
  page_.reset();
  page_.Bind(std::move(page));
  page_.set_disconnect_handler(base::BindOnce(
      &DrivePickerUntrustedHostUI::OnPageDisconnected, base::Unretained(this)));

  page_handler_receiver_.reset();
  page_handler_receiver_.Bind(std::move(handler));

  if (pending_request_) {
    page_->ShowDrivePicker(std::move(pending_request_->result_handler),
                           std::move(pending_request_->keys));
    pending_request_.reset();
  }
}

void DrivePickerUntrustedHostUI::ShowDrivePicker(
    mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>
        result_handler,
    drive_picker_host_untrusted::mojom::DrivePickerKeysPtr keys) {
  if (page_.is_bound() && page_.is_connected()) {
    page_->ShowDrivePicker(std::move(result_handler), std::move(keys));
  } else {
    if (pending_request_) {
      mojo::Remote<drive_picker_host::mojom::DrivePickerResultHandler>(
          std::move(pending_request_->result_handler))
          ->OnCancel();
    }
    pending_request_ = std::make_unique<PendingRequest>(
        std::move(result_handler), std::move(keys));
  }
}

void DrivePickerUntrustedHostUI::OnPageDisconnected() {
  if (pending_request_) {
    mojo::Remote<drive_picker_host::mojom::DrivePickerResultHandler>(
        std::move(pending_request_->result_handler))
        ->OnError(
            drive_picker_host::mojom::DrivePickerError::kMojoDisconnected);
    pending_request_.reset();
  }
}
