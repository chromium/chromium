// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/drive_picker_host/drive_picker_host_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/drive_picker_host/untrusted/drive_picker_host_untrusted_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/drive_picker_host_resources.h"
#include "chrome/grit/drive_picker_host_resources_map.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
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
    : TopChromeWebUIController(web_ui),
      content::WebContentsObserver(web_ui->GetWebContents()) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIDrivePickerHostHost);

  webui::SetupWebUIDataSource(source, kDrivePickerHostResources,
                              IDR_DRIVE_PICKER_HOST_DRIVE_PICKER_HOST_HTML);
}

DrivePickerHostUI::~DrivePickerHostUI() = default;

void DrivePickerHostUI::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  MaybeBindUntrustedBridge(render_frame_host);
}

void DrivePickerHostUI::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->HasCommitted()) {
    MaybeBindUntrustedBridge(navigation_handle->GetRenderFrameHost());
  }
}

void DrivePickerHostUI::MaybeBindUntrustedBridge(
    content::RenderFrameHost* render_frame_host) {
  if (untrusted_bridge_remote_.is_bound() &&
      untrusted_bridge_remote_.is_connected()) {
    return;
  }

  if (render_frame_host && render_frame_host->GetWebUI()) {
    auto* untrusted_ui = render_frame_host->GetWebUI()
                             ->GetController()
                             ->GetAs<DrivePickerUntrustedHostUI>();
    if (untrusted_ui) {
      mojo::PendingRemote<drive_picker_host_untrusted::mojom::DrivePickerBridge>
          bridge;
      untrusted_ui->BindInterface(bridge.InitWithNewPipeAndPassReceiver());
      SetBridge(std::move(bridge));
    }
  }
}

void DrivePickerHostUI::TriggerDrivePickerHost(
    mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>
        result_handler) {
  if (untrusted_bridge_remote_.is_bound() &&
      untrusted_bridge_remote_.is_connected()) {
    untrusted_bridge_remote_->ShowDrivePicker(std::move(result_handler));
  } else {
    // Only the most recent request is kept if the bridge is not yet ready or
    // has been disconnected.
    pending_request_ = std::move(result_handler);
  }
}

void DrivePickerHostUI::SetBridge(
    mojo::PendingRemote<drive_picker_host_untrusted::mojom::DrivePickerBridge>
        untrusted_bridge) {
  untrusted_bridge_remote_.reset();
  untrusted_bridge_remote_.Bind(std::move(untrusted_bridge));

  if (pending_request_) {
    untrusted_bridge_remote_->ShowDrivePicker(std::move(pending_request_));
  }
}

void DrivePickerHostUI::BindInterface(
    mojo::PendingReceiver<drive_picker_host::mojom::DrivePickerHostHandler>
        receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(DrivePickerHostUI)
