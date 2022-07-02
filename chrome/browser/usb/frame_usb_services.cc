// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/frame_usb_services.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/usb/usb_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"

using content::RenderFrameHost;
using content::WebContents;

namespace {

// The renderer performs its own permissions policy checks so a request that
// gets to the browser process indicates malicious code.
const char kPermissionsPolicyViolation[] =
    "Permissions policy blocks access to WebUSB.";

}  // namespace

FrameUsbServices::FrameUsbServices(RenderFrameHost* rfh)
    : content::DocumentUserData<FrameUsbServices>(rfh) {
  // Create UsbTabHelper on creating FrameUsbServices.
  UsbTabHelper::CreateForWebContents(
      WebContents::FromRenderFrameHost(&render_frame_host()));
}

FrameUsbServices::~FrameUsbServices() = default;

void FrameUsbServices::InitializeWebUsbService(
    mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) {
  if (!AllowedByPermissionsPolicy()) {
    mojo::ReportBadMessage(kPermissionsPolicyViolation);
    return;
  }

  if (!web_usb_service_) {
    web_usb_service_ =
        std::make_unique<WebUsbServiceImpl>(&render_frame_host());
  }
  web_usb_service_->BindReceiver(std::move(receiver));
}

bool FrameUsbServices::AllowedByPermissionsPolicy() const {
  return render_frame_host().IsFeatureEnabled(
      blink::mojom::PermissionsPolicyFeature::kUsb);
}

// static
void FrameUsbServices::CreateFrameUsbServices(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) {
  FrameUsbServices::CreateForCurrentDocument(render_frame_host);
  FrameUsbServices::GetForCurrentDocument(render_frame_host)
      ->InitializeWebUsbService(std::move(receiver));
}

DOCUMENT_USER_DATA_KEY_IMPL(FrameUsbServices);
