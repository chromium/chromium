// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/web_usb_service_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/usb/usb_blocklist.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/usb/usb_tab_helper.h"
#include "content/public/browser/browser_thread.h"
#include "media/mojo/mojom/remoting_common.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"

// A UsbDeviceClient represents a UsbDevice pipe that has been passed to the
// renderer process. The UsbDeviceClient pipe allows the browser process to
// continue to monitor how the device is used and cause the connection to be
// closed at will.
class WebUsbServiceImpl::UsbDeviceClient
    : public device::mojom::UsbDeviceClient {
 public:
  UsbDeviceClient(
      WebUsbServiceImpl* service,
      const std::string& device_guid,
      mojo::PendingReceiver<device::mojom::UsbDeviceClient> receiver)
      : service_(service),
        device_guid_(device_guid),
        receiver_(this, std::move(receiver)) {
    receiver_.set_disconnect_handler(
        base::BindOnce(&WebUsbServiceImpl::RemoveDeviceClient,
                       base::Unretained(service_), base::Unretained(this)));
  }

  ~UsbDeviceClient() override {
    if (opened_) {
      // If the connection was opened destroying |receiver_| will cause it to
      // be closed but that event won't be dispatched here because the receiver
      // has been destroyed.
      OnDeviceClosed();
    }
  }

  const std::string& device_guid() const { return device_guid_; }

  // device::mojom::UsbDeviceClient implementation:
  void OnDeviceOpened() override {
    DCHECK(!opened_);
    opened_ = true;
    service_->IncrementConnectionCount();
  }

  void OnDeviceClosed() override {
    DCHECK(opened_);
    opened_ = false;
    service_->DecrementConnectionCount();
  }

 private:
  WebUsbServiceImpl* const service_;
  const std::string device_guid_;
  bool opened_ = false;
  mojo::Receiver<device::mojom::UsbDeviceClient> receiver_;
};

WebUsbServiceImpl::WebUsbServiceImpl(
    content::RenderFrameHost* render_frame_host,
    base::WeakPtr<WebUsbChooser> usb_chooser)
    : render_frame_host_(render_frame_host),
      usb_chooser_(std::move(usb_chooser)),
      device_observer_(this),
      permission_observer_(this) {
  DCHECK(render_frame_host_);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);
  // This class is destroyed on cross-origin navigations and so it is safe to
  // cache these values.
  requesting_origin_ = render_frame_host_->GetLastCommittedOrigin();
  embedding_origin_ = web_contents->GetMainFrame()->GetLastCommittedOrigin();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  chooser_context_ = UsbChooserContextFactory::GetForProfile(profile);
  DCHECK(chooser_context_);

  receivers_.set_disconnect_handler(base::BindRepeating(
      &WebUsbServiceImpl::OnConnectionError, base::Unretained(this)));
}

WebUsbServiceImpl::~WebUsbServiceImpl() = default;

void WebUsbServiceImpl::BindReceiver(
    mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) {
  receivers_.Add(this, std::move(receiver));

  // Listen to UsbChooserContext for add/remove device events from UsbService.
  // We can't set WebUsbServiceImpl as a UsbDeviceManagerClient because
  // the OnDeviceRemoved event will be delivered here after it is delivered
  // to UsbChooserContext, meaning that all ephemeral permission checks in
  // OnDeviceRemoved() will fail.
  if (!device_observer_.IsObservingSources())
    device_observer_.Add(chooser_context_);
  if (!permission_observer_.IsObservingSources())
    permission_observer_.Add(chooser_context_);
}

bool WebUsbServiceImpl::HasDevicePermission(
    const device::mojom::UsbDeviceInfo& device_info) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(render_frame_host_);
  if (!chooser_context_)
    return false;

  return chooser_context_->HasDevicePermission(requesting_origin_,
                                               embedding_origin_, device_info);
}

void WebUsbServiceImpl::GetDevices(GetDevicesCallback callback) {
  if (!chooser_context_) {
    std::move(callback).Run(std::vector<device::mojom::UsbDeviceInfoPtr>());
    return;
  }

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
    mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver) {
  if (!chooser_context_)
    return;

  auto* device_info = chooser_context_->GetDeviceInfo(guid);
  if (!device_info || !HasDevicePermission(*device_info))
    return;

  // Connect Blink to the native device and keep a receiver to this for the
  // UsbDeviceClient interface so we can receive DeviceOpened/Closed events.
  // This receiver will also be closed to notify the device service to close
  // the connection if permission is revoked.
  mojo::PendingRemote<device::mojom::UsbDeviceClient> device_client;
  device_clients_.push_back(std::make_unique<UsbDeviceClient>(
      this, guid, device_client.InitWithNewPipeAndPassReceiver()));
  chooser_context_->GetDevice(guid, std::move(device_receiver),
                              std::move(device_client));
}

void WebUsbServiceImpl::GetPermission(
    std::vector<device::mojom::UsbDeviceFilterPtr> device_filters,
    GetPermissionCallback callback) {
  if (!usb_chooser_) {
    std::move(callback).Run(nullptr);
    return;
  }

  usb_chooser_->GetPermission(std::move(device_filters), std::move(callback));
}

void WebUsbServiceImpl::SetClient(
    mojo::PendingAssociatedRemote<device::mojom::UsbDeviceManagerClient>
        client) {
  DCHECK(client);
  clients_.Add(std::move(client));
}

void WebUsbServiceImpl::OnPermissionRevoked(
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  if (requesting_origin_ != requesting_origin ||
      embedding_origin_ != embedding_origin) {
    return;
  }

  // Close the connection between Blink and the device if the device lost
  // permission.
  base::EraseIf(device_clients_, [this](const auto& client) {
    auto* device_info = chooser_context_->GetDeviceInfo(client->device_guid());
    if (!device_info)
      return true;

    return !HasDevicePermission(*device_info);
  });
}

void WebUsbServiceImpl::OnDeviceAdded(
    const device::mojom::UsbDeviceInfo& device_info) {
  if (!HasDevicePermission(device_info))
    return;

  for (auto& client : clients_)
    client->OnDeviceAdded(device_info.Clone());
}

void WebUsbServiceImpl::OnDeviceRemoved(
    const device::mojom::UsbDeviceInfo& device_info) {
  base::EraseIf(device_clients_, [&device_info](const auto& client) {
    return device_info.guid == client->device_guid();
  });

  if (!HasDevicePermission(device_info))
    return;

  for (auto& client : clients_)
    client->OnDeviceRemoved(device_info.Clone());
}

void WebUsbServiceImpl::OnDeviceManagerConnectionError() {
  // Close the connection with blink.
  clients_.Clear();
  receivers_.Clear();

  // Remove itself from UsbChooserContext's ObserverList.
  device_observer_.RemoveAll();
  permission_observer_.RemoveAll();
}

// device::mojom::UsbDeviceClient implementation:
void WebUsbServiceImpl::IncrementConnectionCount() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);
  UsbTabHelper* tab_helper = UsbTabHelper::FromWebContents(web_contents);
  tab_helper->IncrementConnectionCount();
}

void WebUsbServiceImpl::DecrementConnectionCount() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);
  UsbTabHelper* tab_helper = UsbTabHelper::FromWebContents(web_contents);
  tab_helper->DecrementConnectionCount();
}

void WebUsbServiceImpl::RemoveDeviceClient(const UsbDeviceClient* client) {
  base::EraseIf(device_clients_, [client](const auto& this_client) {
    return client == this_client.get();
  });
}

void WebUsbServiceImpl::OnConnectionError() {
  if (receivers_.empty()) {
    device_observer_.RemoveAll();
    permission_observer_.RemoveAll();
  }
}
