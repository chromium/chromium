// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/web_usb_chooser_desktop.h"

#include <utility>

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_bubble_manager.h"
#include "chrome/browser/ui/permission_bubble/chooser_bubble_delegate.h"
#include "chrome/browser/usb/usb_chooser_controller.h"
#include "components/bubble/bubble_controller.h"
#include "content/public/browser/web_contents.h"

WebUsbChooserDesktop::WebUsbChooserDesktop(
    content::RenderFrameHost* render_frame_host)
    : WebUsbChooser(render_frame_host) {}

WebUsbChooserDesktop::~WebUsbChooserDesktop() {
  if (bubble_)
    bubble_->CloseBubble(BUBBLE_CLOSE_FORCED);
}

void WebUsbChooserDesktop::ShowChooser(
    std::unique_ptr<UsbChooserController> controller) {
  // Only one chooser bubble may be shown at a time.
  if (bubble_)
    bubble_->CloseBubble(BUBBLE_CLOSE_FORCED);

  auto delegate = std::make_unique<ChooserBubbleDelegate>(
      render_frame_host(), std::move(controller));
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host());
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser)
    bubble_ = browser->GetBubbleManager()->ShowBubble(std::move(delegate));
}

base::WeakPtr<WebUsbChooser> WebUsbChooserDesktop::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}
