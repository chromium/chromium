// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/bluetooth_delegate_impl.h"

#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "components/permissions/contexts/bluetooth_chooser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "third_party/blink/public/common/bluetooth/web_bluetooth_device_id.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"

using blink::WebBluetoothDeviceId;
using content::RenderFrameHost;
using device::BluetoothUUID;

namespace permissions {

BluetoothDelegateImpl::BluetoothDelegateImpl(std::unique_ptr<Client> client)
    : client_(std::move(client)) {}

BluetoothDelegateImpl::~BluetoothDelegateImpl() = default;

std::unique_ptr<content::BluetoothChooser>
BluetoothDelegateImpl::RunBluetoothChooser(
    content::RenderFrameHost* frame,
    const content::BluetoothChooser::EventHandler& event_handler) {
  return client_->RunBluetoothChooser(frame, event_handler);
}

std::unique_ptr<content::BluetoothScanningPrompt>
BluetoothDelegateImpl::ShowBluetoothScanningPrompt(
    content::RenderFrameHost* frame,
    const content::BluetoothScanningPrompt::EventHandler& event_handler) {
  return client_->ShowBluetoothScanningPrompt(frame, event_handler);
}

void BluetoothDelegateImpl::ShowDevicePairPrompt(
    RenderFrameHost* frame,
    const std::u16string& device_identifier,
    PairPromptCallback callback,
    PairingKind pairing_kind,
    const std::optional<std::u16string>& pin) {
  client_->ShowBluetoothDevicePairDialog(
      frame, device_identifier, std::move(callback), pairing_kind, pin);
}

WebBluetoothDeviceId BluetoothDelegateImpl::GetWebBluetoothDeviceId(
    RenderFrameHost* frame,
    const std::string& device_address) {
  return client_->GetBluetoothChooserContext(frame)->GetWebBluetoothDeviceId(
      frame->GetMainFrame()->GetLastCommittedOrigin(), device_address);
}

std::string BluetoothDelegateImpl::GetDeviceAddress(
    RenderFrameHost* frame,
    const WebBluetoothDeviceId& device_id) {
  return client_->GetBluetoothChooserContext(frame)->GetDeviceAddress(
      frame->GetMainFrame()->GetLastCommittedOrigin(), device_id);
}

WebBluetoothDeviceId BluetoothDelegateImpl::AddScannedDevice(
    RenderFrameHost* frame,
    const std::string& device_address) {
  return client_->GetBluetoothChooserContext(frame)->AddScannedDevice(
      frame->GetMainFrame()->GetLastCommittedOrigin(), device_address);
}

WebBluetoothDeviceId BluetoothDelegateImpl::GrantServiceAccessPermission(
    RenderFrameHost* frame,
    const device::BluetoothDevice* device,
    const blink::mojom::WebBluetoothRequestDeviceOptions* options) {
  return client_->GetBluetoothChooserContext(frame)
      ->GrantServiceAccessPermission(
          frame->GetMainFrame()->GetLastCommittedOrigin(), device, options);
}

bool BluetoothDelegateImpl::HasDevicePermission(
    RenderFrameHost* frame,
    const WebBluetoothDeviceId& device_id) {
  return client_->GetBluetoothChooserContext(frame)->HasDevicePermission(
      frame->GetMainFrame()->GetLastCommittedOrigin(), device_id);
}

void BluetoothDelegateImpl::RevokeDevicePermissionWebInitiated(
    RenderFrameHost* frame,
    const WebBluetoothDeviceId& device_id) {
  client_->GetBluetoothChooserContext(frame)
      ->RevokeDevicePermissionWebInitiated(
          frame->GetMainFrame()->GetLastCommittedOrigin(), device_id);
}

bool BluetoothDelegateImpl::MayUseBluetooth(RenderFrameHost* frame) {
  return true;
}

bool BluetoothDelegateImpl::IsAllowedToAccessService(
    RenderFrameHost* frame,
    const WebBluetoothDeviceId& device_id,
    const BluetoothUUID& service) {
  return client_->GetBluetoothChooserContext(frame)->IsAllowedToAccessService(
      frame->GetMainFrame()->GetLastCommittedOrigin(), device_id, service);
}

bool BluetoothDelegateImpl::IsAllowedToAccessAtLeastOneService(
    RenderFrameHost* frame,
    const WebBluetoothDeviceId& device_id) {
  return client_->GetBluetoothChooserContext(frame)
      ->IsAllowedToAccessAtLeastOneService(
          frame->GetMainFrame()->GetLastCommittedOrigin(), device_id);
}

bool BluetoothDelegateImpl::IsAllowedToAccessManufacturerData(
    RenderFrameHost* frame,
    const WebBluetoothDeviceId& device_id,
    uint16_t manufacturer_code) {
  return client_->GetBluetoothChooserContext(frame)
      ->IsAllowedToAccessManufacturerData(
          frame->GetMainFrame()->GetLastCommittedOrigin(), device_id,
          manufacturer_code);
}

void BluetoothDelegateImpl::AddFramePermissionObserver(
    FramePermissionObserver* observer) {
  std::unique_ptr<ChooserContextPermissionObserver>& chooser_observer =
      chooser_observers_[observer->GetRenderFrameHost()];
  if (!chooser_observer) {
    chooser_observer = std::make_unique<ChooserContextPermissionObserver>(
        this,
        client_->GetBluetoothChooserContext(observer->GetRenderFrameHost()));
  }

  chooser_observer->AddFramePermissionObserver(observer);
}

void BluetoothDelegateImpl::RemoveFramePermissionObserver(
    FramePermissionObserver* observer) {
  auto it = chooser_observers_.find(observer->GetRenderFrameHost());
  if (it == chooser_observers_.end())
    return;
  it->second->RemoveFramePermissionObserver(observer);
}

std::vector<blink::mojom::WebBluetoothDevicePtr>
BluetoothDelegateImpl::GetPermittedDevices(content::RenderFrameHost* frame) {
  auto* context = client_->GetBluetoothChooserContext(frame);
  std::vector<std::unique_ptr<ObjectPermissionContextBase::Object>> objects =
      context->GetGrantedObjects(
          frame->GetMainFrame()->GetLastCommittedOrigin());
  std::vector<blink::mojom::WebBluetoothDevicePtr> permitted_devices;

  for (const auto& object : objects) {
    auto permitted_device = blink::mojom::WebBluetoothDevice::New();
    permitted_device->id =
        BluetoothChooserContext::GetObjectDeviceId(object->value);
    permitted_device->name =
        base::UTF16ToUTF8(context->GetObjectDisplayName(object->value));
    permitted_devices.push_back(std::move(permitted_device));
  }

  return permitted_devices;
}

BluetoothDelegateImpl::ChooserContextPermissionObserver::
    ChooserContextPermissionObserver(BluetoothDelegateImpl* owning_delegate,
                                     ObjectPermissionContextBase* context)
    : owning_delegate_(owning_delegate) {
  observer_.Observe(context);
}

BluetoothDelegateImpl::ChooserContextPermissionObserver::
    ~ChooserContextPermissionObserver() = default;

void BluetoothDelegateImpl::ChooserContextPermissionObserver::
    OnPermissionRevoked(const url::Origin& origin) {
  observers_pending_removal_.clear();
  is_traversing_observers_ = true;

  for (auto& observer : observer_list_)
    observer.OnPermissionRevoked(origin);

  is_traversing_observers_ = false;
  for (FramePermissionObserver* observer : observers_pending_removal_)
    RemoveFramePermissionObserver(observer);
}

void BluetoothDelegateImpl::ChooserContextPermissionObserver::
    AddFramePermissionObserver(FramePermissionObserver* observer) {
  observer_list_.AddObserver(observer);
}

void BluetoothDelegateImpl::ChooserContextPermissionObserver::
    RemoveFramePermissionObserver(FramePermissionObserver* observer) {
  if (is_traversing_observers_) {
    observers_pending_removal_.emplace_back(observer);
    return;
  }

  observer_list_.RemoveObserver(observer);
  if (observer_list_.empty())
    owning_delegate_->chooser_observers_.erase(observer->GetRenderFrameHost());
  // Previous call destructed this instance. Don't add code after this.
}

}  // namespace permissions
