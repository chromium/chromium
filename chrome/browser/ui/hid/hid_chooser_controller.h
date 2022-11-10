// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HID_HID_CHOOSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_HID_HID_CHOOSER_CONTROLLER_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/hid/hid_chooser_context.h"
#include "components/permissions/chooser_controller.h"
#include "content/public/browser/hid_chooser.h"
#include "content/public/browser/weak_document_ptr.h"
#include "services/device/public/mojom/hid.mojom-forward.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/hid/hid.mojom.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;
}  // namespace content

// HidChooserController provides data for the WebHID API permission prompt.
class HidChooserController : public permissions::ChooserController,
                             public HidChooserContext::DeviceObserver {
 public:
  // Construct a chooser controller for Human Interface Devices (HID).
  // |render_frame_host| is used to initialize the chooser strings and to access
  // the requesting and embedding origins. |callback| is called when the chooser
  // is closed, either by selecting an item or by dismissing the chooser dialog.
  // The callback is called with the selected device, or nullptr if no device is
  // selected.
  HidChooserController(
      content::RenderFrameHost* render_frame_host,
      std::vector<blink::mojom::HidDeviceFilterPtr> filters,
      std::vector<blink::mojom::HidDeviceFilterPtr> exclusion_filters,
      content::HidChooser::Callback callback);
  HidChooserController(HidChooserController&) = delete;
  HidChooserController& operator=(HidChooserController&) = delete;
  ~HidChooserController() override;

  // permissions::ChooserController:
  bool ShouldShowHelpButton() const override;
  std::u16string GetNoOptionsText() const override;
  std::u16string GetOkButtonLabel() const override;
  std::pair<std::u16string, std::u16string> GetThrobberLabelAndTooltip()
      const override;
  size_t NumOptions() const override;
  std::u16string GetOption(size_t index) const override;
  bool IsPaired(size_t index) const override;
  void Select(const std::vector<size_t>& indices) override;
  void Cancel() override;
  void Close() override;
  void OpenHelpCenterUrl() const override;

  // HidChooserContext::DeviceObserver:
  void OnDeviceAdded(const device::mojom::HidDeviceInfo& device_info) override;
  void OnDeviceRemoved(
      const device::mojom::HidDeviceInfo& device_info) override;
  void OnDeviceChanged(
      const device::mojom::HidDeviceInfo& device_info) override;
  void OnHidManagerConnectionError() override;
  void OnHidChooserContextShutdown() override;

 private:
  void OnGotDevices(std::vector<device::mojom::HidDeviceInfoPtr> devices);
  bool DisplayDevice(const device::mojom::HidDeviceInfo& device) const;
  bool FilterMatchesAny(const device::mojom::HidDeviceInfo& device) const;
  bool IsExcluded(const device::mojom::HidDeviceInfo& device) const;
  void AddMessageToConsole(blink::mojom::ConsoleMessageLevel level,
                           const std::string& message) const;

  // Add |device_info| to |device_map_|. The device is added to the chooser item
  // representing the physical device. If the chooser item does not yet exist, a
  // new item is appended. Returns true if an item was appended.
  bool AddDeviceInfo(const device::mojom::HidDeviceInfo& device_info);

  // Remove |device_info| from |device_map_|. The device info is removed from
  // the chooser item representing the physical device. If this would cause the
  // item to be empty, the chooser item is removed. Does nothing if the device
  // is not in the chooser item. Returns true if an item was removed.
  bool RemoveDeviceInfo(const device::mojom::HidDeviceInfo& device_info);

  // Update the information for the device described by |device_info| in the
  // |device_map_|.
  void UpdateDeviceInfo(const device::mojom::HidDeviceInfo& device_info);

  std::vector<blink::mojom::HidDeviceFilterPtr> filters_;
  std::vector<blink::mojom::HidDeviceFilterPtr> exclusion_filters_;
  content::HidChooser::Callback callback_;
  content::WeakDocumentPtr initiator_document_;
  const url::Origin origin_;

  // The lifetime of the chooser context is tied to the browser context used to
  // create it, and may be destroyed while the chooser is still active.
  base::WeakPtr<HidChooserContext> chooser_context_;

  // Information about connected devices and their HID interfaces. A single
  // physical device may expose multiple HID interfaces. Keys are physical
  // device IDs, values are collections of HidDeviceInfo objects representing
  // the HID interfaces hosted by the physical device.
  std::map<std::string, std::vector<device::mojom::HidDeviceInfoPtr>>
      device_map_;

  // An ordered list of physical device IDs that determines the order of items
  // in the chooser.
  std::vector<std::string> items_;

  base::ScopedObservation<HidChooserContext, HidChooserContext::DeviceObserver>
      observation_{this};

  base::WeakPtrFactory<HidChooserController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_HID_HID_CHOOSER_CONTROLLER_H_
