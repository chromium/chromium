// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/frame_usb_services.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/usb/usb_tab_helper.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/usb/web_usb_chooser_android.h"
#else
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/usb/web_usb_chooser_desktop.h"
#endif  // BUILDFLAG(IS_ANDROID)

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

void FrameUsbServices::InitializeWebUsbChooser() {
  if (!usb_chooser_) {
#if BUILDFLAG(IS_ANDROID)
    usb_chooser_ = std::make_unique<WebUsbChooserAndroid>(&render_frame_host());
#else
    usb_chooser_ = std::make_unique<WebUsbChooserDesktop>(&render_frame_host());
#endif  // BUILDFLAG(IS_ANDROID)
  }
}

void FrameUsbServices::InitializeWebUsbService(
    mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) {
  if (!AllowedByPermissionsPolicy()) {
    mojo::ReportBadMessage(kPermissionsPolicyViolation);
    return;
  }

  InitializeWebUsbChooser();
  if (!web_usb_service_) {
    web_usb_service_ = std::make_unique<WebUsbServiceImpl>(
        &render_frame_host(), usb_chooser_->GetWeakPtr());
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
