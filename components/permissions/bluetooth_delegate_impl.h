// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_BLUETOOTH_DELEGATE_IMPL_H_
#define COMPONENTS_PERMISSIONS_BLUETOOTH_DELEGATE_IMPL_H_

#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/permissions/object_permission_context_base.h"
#include "content/public/browser/bluetooth_delegate.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom-forward.h"

namespace blink {
class WebBluetoothDeviceId;
}  // namespace blink

namespace content {
class RenderFrameHost;
}  // namespace content

namespace device {
class BluetoothDevice;
class BluetoothUUID;
}  // namespace device

namespace permissions {

class BluetoothChooserContext;

// Provides an interface for managing device permissions for Web Bluetooth and
// Web Bluetooth Scanning API.
class BluetoothDelegateImpl : public content::BluetoothDelegate {
 public:
  // Provides embedder-level functionality to BluetoothDelegateImpl.
  class Client {
   public:
    Client() = default;
    virtual ~Client() = default;

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    // Provides access to a BluetoothChooserContext without transferring
    // ownership.
    virtual permissions::BluetoothChooserContext* GetBluetoothChooserContext(
        content::RenderFrameHost* frame) = 0;

    // See content::BluetoothDelegate::RunBluetoothChooser.
    virtual std::unique_ptr<content::BluetoothChooser> RunBluetoothChooser(
        content::RenderFrameHost* frame,
        const content::BluetoothChooser::EventHandler& event_handler) = 0;

    // See content::BluetoothDelegate::ShowBluetoothScanningPrompt.
    virtual std::unique_ptr<content::BluetoothScanningPrompt>
    ShowBluetoothScanningPrompt(
        content::RenderFrameHost* frame,
        const content::BluetoothScanningPrompt::EventHandler&
            event_handler) = 0;

    // Prompt the user for pairing Bluetooth device.
    //
    // The |device_identifier| is a localized string (device name, address,
    // etc.) displayed to the user for identification purposes. When the
    // prompt is complete |callback| is called with the result.
    // |pairing_kind| is to determine which pairing kind of prompt should be
    // shown.
    virtual void ShowBluetoothDevicePairDialog(
        content::RenderFrameHost* frame,
        const std::u16string& device_identifier,
        content::BluetoothDelegate::PairPromptCallback callback,
        content::BluetoothDelegate::PairingKind pairing_kind,
        const std::optional<std::u16string>& pin) = 0;
  };

  explicit BluetoothDelegateImpl(std::unique_ptr<Client> client);
  ~BluetoothDelegateImpl() override;

  BluetoothDelegateImpl(const BluetoothDelegateImpl&) = delete;
  BluetoothDelegateImpl& operator=(const BluetoothDelegateImpl&) = delete;

  // BluetoothDelegate implementation:
  std::unique_ptr<content::BluetoothChooser> RunBluetoothChooser(
      content::RenderFrameHost* frame,
      const content::BluetoothChooser::EventHandler& event_handler) override;
  std::unique_ptr<content::BluetoothScanningPrompt> ShowBluetoothScanningPrompt(
      content::RenderFrameHost* frame,
      const content::BluetoothScanningPrompt::EventHandler& event_handler)
      override;

  void ShowDevicePairPrompt(content::RenderFrameHost* frame,
                            const std::u16string& device_identifier,
                            PairPromptCallback callback,
                            PairingKind pairing_kind,
                            const std::optional<std::u16string>& pin) override;

  blink::WebBluetoothDeviceId GetWebBluetoothDeviceId(
      content::RenderFrameHost* frame,
      const std::string& device_address) override;
  std::string GetDeviceAddress(
      content::RenderFrameHost* frame,
      const blink::WebBluetoothDeviceId& device_id) override;
  blink::WebBluetoothDeviceId AddScannedDevice(
      content::RenderFrameHost* frame,
      const std::string& device_address) override;
  blink::WebBluetoothDeviceId GrantServiceAccessPermission(
      content::RenderFrameHost* frame,
      const device::BluetoothDevice* device,
      const blink::mojom::WebBluetoothRequestDeviceOptions* options) override;
  bool HasDevicePermission(
      content::RenderFrameHost* frame,
      const blink::WebBluetoothDeviceId& device_id) override;
  void RevokeDevicePermissionWebInitiated(
      content::RenderFrameHost* frame,
      const blink::WebBluetoothDeviceId& device_id) override;
  bool MayUseBluetooth(content::RenderFrameHost* frame) override;
  bool IsAllowedToAccessService(content::RenderFrameHost* frame,
                                const blink::WebBluetoothDeviceId& device_id,
                                const device::BluetoothUUID& service) override;
  bool IsAllowedToAccessAtLeastOneService(
      content::RenderFrameHost* frame,
      const blink::WebBluetoothDeviceId& device_id) override;
  bool IsAllowedToAccessManufacturerData(
      content::RenderFrameHost* frame,
      const blink::WebBluetoothDeviceId& device_id,
      uint16_t manufacturer_code) override;
  std::vector<blink::mojom::WebBluetoothDevicePtr> GetPermittedDevices(
      content::RenderFrameHost* frame) override;
  void AddFramePermissionObserver(FramePermissionObserver* observer) override;
  void RemoveFramePermissionObserver(
      FramePermissionObserver* observer) override;

 private:
  // Manages the FramePermissionObserver list for a particular RFH. Will
  // self-delete when the last observer is removed from the |owning_delegate|'s
  // |chooser_observers_| map.
  class ChooserContextPermissionObserver
      : public ObjectPermissionContextBase::PermissionObserver {
   public:
    explicit ChooserContextPermissionObserver(
        BluetoothDelegateImpl* owning_delegate,
        ObjectPermissionContextBase* context);
    ~ChooserContextPermissionObserver() override;

    ChooserContextPermissionObserver(const ChooserContextPermissionObserver&) =
        delete;
    ChooserContextPermissionObserver& operator=(
        const ChooserContextPermissionObserver) = delete;

    // ObjectPermissionContextBase::PermissionObserver:
    void OnPermissionRevoked(const url::Origin& origin) override;

    void AddFramePermissionObserver(FramePermissionObserver* observer);
    void RemoveFramePermissionObserver(FramePermissionObserver* observer);

   private:
    raw_ptr<BluetoothDelegateImpl> owning_delegate_;
    base::ObserverList<FramePermissionObserver> observer_list_;
    std::list<raw_ptr<FramePermissionObserver, CtnExperimental>>
        observers_pending_removal_;
    bool is_traversing_observers_ = false;
    base::ScopedObservation<ObjectPermissionContextBase,
                            ObjectPermissionContextBase::PermissionObserver>
        observer_{this};
  };

  std::unique_ptr<Client> client_;

  std::map<content::RenderFrameHost*,
           std::unique_ptr<ChooserContextPermissionObserver>>
      chooser_observers_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_BLUETOOTH_DELEGATE_IMPL_H_
