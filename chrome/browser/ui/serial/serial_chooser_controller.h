// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SERIAL_SERIAL_CHOOSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_SERIAL_SERIAL_CHOOSER_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/serial/serial_chooser_context.h"
#include "components/permissions/chooser_controller.h"
#include "content/public/browser/serial_chooser.h"
#include "content/public/browser/weak_document_ptr.h"
#include "services/device/public/mojom/serial.mojom-forward.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/serial/serial.mojom.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;
}  // namespace content

// SerialChooserController provides data for the Serial API permission prompt.
class SerialChooserController final
    : public permissions::ChooserController,
      public SerialChooserContext::PortObserver {
 public:
  SerialChooserController(
      content::RenderFrameHost* render_frame_host,
      std::vector<blink::mojom::SerialPortFilterPtr> filters,
      std::vector<::device::BluetoothUUID> allowed_bluetooth_service_class_ids,
      content::SerialChooser::Callback callback);

  SerialChooserController(const SerialChooserController&) = delete;
  SerialChooserController& operator=(const SerialChooserController&) = delete;

  ~SerialChooserController() override;

  const device::mojom::SerialPortInfo& GetPortForTest(size_t index) const;

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

  // SerialChooserContext::PortObserver:
  void OnPortAdded(const device::mojom::SerialPortInfo& port) override;
  void OnPortRemoved(const device::mojom::SerialPortInfo& port) override;
  void OnPortManagerConnectionError() override;
  void OnPermissionRevoked(const url::Origin& origin) override {}

 private:
  void OnGetDevices(std::vector<device::mojom::SerialPortInfoPtr> ports);
  bool DisplayDevice(const device::mojom::SerialPortInfo& port) const;
  void AddMessageToConsole(blink::mojom::ConsoleMessageLevel level,
                           const std::string& message) const;
  void RunCallback(device::mojom::SerialPortInfoPtr port);
  bool DisplayServiceClassId(const device::mojom::SerialPortInfo& port) const;

  std::vector<blink::mojom::SerialPortFilterPtr> filters_;
  std::vector<::device::BluetoothUUID> allowed_bluetooth_service_class_ids_;
  content::SerialChooser::Callback callback_;
  content::WeakDocumentPtr initiator_document_;
  url::Origin origin_;

  base::WeakPtr<SerialChooserContext> chooser_context_;
  base::ScopedObservation<SerialChooserContext,
                          SerialChooserContext::PortObserver>
      observation_{this};

  std::vector<device::mojom::SerialPortInfoPtr> ports_;

  base::WeakPtrFactory<SerialChooserController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_SERIAL_SERIAL_CHOOSER_CONTROLLER_H_
