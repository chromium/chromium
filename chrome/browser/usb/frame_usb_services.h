// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_FRAME_USB_SERVICES_H_
#define CHROME_BROWSER_USB_FRAME_USB_SERVICES_H_

#include "chrome/browser/usb/web_usb_service_impl.h"
#include "content/public/browser/render_document_host_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace blink {
namespace mojom {
class WebUsbService;
}
}  // namespace blink

class WebUsbChooser;

// Collection of USB-related document-associated services (e.g.
// WebUsbServiceImpl) with the lifetime bound to the lifetime of the document.
class FrameUsbServices
    : public content::RenderDocumentHostUserData<FrameUsbServices> {
 public:
  ~FrameUsbServices() override;

  static void CreateFrameUsbServices(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::WebUsbService> receiver);

 private:
  explicit FrameUsbServices(content::RenderFrameHost* render_frame_host);

  friend class content::RenderDocumentHostUserData<FrameUsbServices>;

  void InitializeWebUsbChooser();

  void InitializeWebUsbService(
      mojo::PendingReceiver<blink::mojom::WebUsbService> receiver);

  bool AllowedByFeaturePolicy() const;

  std::unique_ptr<WebUsbChooser> usb_chooser_;
  std::unique_ptr<WebUsbServiceImpl> web_usb_service_;

  content::RenderFrameHost* render_frame_host_;

  RENDER_DOCUMENT_HOST_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_USB_FRAME_USB_SERVICES_H_
