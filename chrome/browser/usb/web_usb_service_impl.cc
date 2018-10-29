// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/web_usb_service_impl.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/usb/usb_blocklist.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/usb/usb_tab_helper.h"
#include "content/public/browser/browser_thread.h"

WebUsbServiceImpl::WebUsbServiceImpl(
    content::RenderFrameHost* render_frame_host,
    base::WeakPtr<WebUsbChooser> usb_chooser)
    : render_frame_host_(render_frame_host),
      usb_chooser_(std::move(usb_chooser)),
      observer_(this),
      weak_factory_(this) {
  DCHECK(render_frame_host_);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  chooser_context_ = UsbChooserContextFactory::GetForProfile(profile);
  DCHECK(chooser_context_);

  bindings_.set_connection_error_handler(base::BindRepeating(
      &WebUsbServiceImpl::OnBindingConnectionError, base::Unretained(this)));
}

WebUsbServiceImpl::~WebUsbServiceImpl() = default;

void WebUsbServiceImpl::BindRequest(
    blink::mojom::WebUsbServiceRequest request) {
  bindings_.AddBinding(this, std::move(request));

  // Listen to UsbChooserContext for add/remove device events from UsbService.
  // We can't set WebUsbServiceImpl as a UsbDeviceManagerClient because
  // the OnDeviceRemoved event will be delivered here after it is delivered
  // to UsbChooserContext, meaning that all ephemeral permission checks in
  // OnDeviceRemoved() will fail.
  if (!observer_.IsObservingSources())
    observer_.Add(chooser_context_);
}

bool WebUsbServiceImpl::HasDevicePermission(
    const device::mojom::UsbDeviceInfo& device_info) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(render_frame_host_);
  if (!chooser_context_)
    return false;

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);
  content::RenderFrameHost* main_frame = web_contents->GetMainFrame();

  return chooser_context_->HasDevicePermission(
      render_frame_host_->GetLastCommittedURL().GetOrigin(),
      main_frame->GetLastCommittedURL().GetOrigin(), device_info);
}

void WebUsbServiceImpl::GetDevices(GetDevicesCallback callback) {
  if (!chooser_context_)
    std::move(callback).Run(std::vector<device::mojom::UsbDeviceInfoPtr>());

  chooser_context_->GetDevices(base::BindOnce(&WebUsbServiceImpl::OnGetDevices,
                                              weak_factory_.GetWeakPtr(),
                                              std::move(callback)));
}

void WebUsbServiceImpl::OnGetDevices(
    GetDevicesCallback callback,
    std::vector<device::mojom::UsbDeviceInfoPtr> device_info_list) {
  std::vector<device::mojom::UsbDeviceInfoPtr> device_infos;
  for (auto& device_info : device_info_list) {
    if (HasDevicePermission(*device_info))
      device_infos.push_back(device_info.Clone());
  }
  std::move(callback).Run(std::move(device_infos));
}

void WebUsbServiceImpl::GetDevice(
    const std::string& guid,
    device::mojom::UsbDeviceRequest device_request) {
  if (!chooser_context_)
    return;

  // Try to bind with the new device to be created for DeviceOpened/Closed
  // events. It is safe to pass this request directly to UsbDeviceManager
  // because |guid| is unguessable.
  device::mojom::UsbDeviceClientPtr device_client;
  device_client_bindings_.AddBinding(this, mojo::MakeRequest(&device_client));
  chooser_context_->GetDevice(guid, std::move(device_request),
                              std::move(device_client));
}

void WebUsbServiceImpl::GetPermission(
    std::vector<device::mojom::UsbDeviceFilterPtr> device_filters,
    GetPermissionCallback callback) {
  if (!usb_chooser_)
    std::move(callback).Run(nullptr);

  usb_chooser_->GetPermission(std::move(device_filters), std::move(callback));
}

void WebUsbServiceImpl::SetClient(
    device::mojom::UsbDeviceManagerClientAssociatedPtrInfo client) {
  DCHECK(client);

  device::mojom::UsbDeviceManagerClientAssociatedPtr client_ptr;
  client_ptr.Bind(std::move(client));
  clients_.AddPtr(std::move(client_ptr));
}

void WebUsbServiceImpl::OnDeviceAdded(
    const device::mojom::UsbDeviceInfo& device_info) {
  if (!HasDevicePermission(device_info))
    return;

  clients_.ForAllPtrs(
      [&device_info](device::mojom::UsbDeviceManagerClient* client) {
        client->OnDeviceAdded(device_info.Clone());
      });
}

void WebUsbServiceImpl::OnDeviceRemoved(
    const device::mojom::UsbDeviceInfo& device_info) {
  if (!HasDevicePermission(device_info))
    return;

  clients_.ForAllPtrs(
      [&device_info](device::mojom::UsbDeviceManagerClient* client) {
        client->OnDeviceRemoved(device_info.Clone());
      });
}

void WebUsbServiceImpl::OnDeviceManagerConnectionError() {
  // Close the connection with blink.
  clients_.CloseAll();
  bindings_.CloseAllBindings();

  // Remove itself from UsbChooserContext's ObserverList.
  observer_.RemoveAll();
}

// device::mojom::UsbDeviceClient implementation:
void WebUsbServiceImpl::OnDeviceOpened() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);
  UsbTabHelper* tab_helper = UsbTabHelper::FromWebContents(web_contents);
  tab_helper->IncrementConnectionCount(render_frame_host_);
}

void WebUsbServiceImpl::OnDeviceClosed() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);
  UsbTabHelper* tab_helper = UsbTabHelper::FromWebContents(web_contents);
  tab_helper->DecrementConnectionCount(render_frame_host_);
}

void WebUsbServiceImpl::OnBindingConnectionError() {
  if (bindings_.empty())
    observer_.RemoveAll();
}
