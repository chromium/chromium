// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SERIAL_SERIAL_CHOOSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_SERIAL_SERIAL_CHOOSER_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chooser_controller/chooser_controller.h"
#include "chrome/browser/serial/serial_chooser_context.h"
#include "content/public/browser/serial_chooser.h"
#include "services/device/public/mojom/serial.mojom-forward.h"
#include "third_party/blink/public/mojom/serial/serial.mojom.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;
}  // namespace content

// SerialChooserController provides data for the Serial API permission prompt.
class SerialChooserController final
    : public ChooserController,
      public SerialChooserContext::PortObserver {
 public:
  SerialChooserController(
      content::RenderFrameHost* render_frame_host,
      std::vector<blink::mojom::SerialPortFilterPtr> filters,
      content::SerialChooser::Callback callback);
  ~SerialChooserController() override;

  // ChooserController:
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

 private:
  void OnGetDevices(std::vector<device::mojom::SerialPortInfoPtr> ports);
  bool DisplayDevice(const device::mojom::SerialPortInfo& port) const;
  void RunCallback(device::mojom::SerialPortInfoPtr port);

  std::vector<blink::mojom::SerialPortFilterPtr> filters_;
  content::SerialChooser::Callback callback_;
  url::Origin origin_;
  const int frame_tree_node_id_;

  base::WeakPtr<SerialChooserContext> chooser_context_;
  ScopedObserver<SerialChooserContext,
                 SerialChooserContext::PortObserver,
                 &SerialChooserContext::AddPortObserver,
                 &SerialChooserContext::RemovePortObserver>
      observer_{this};

  std::vector<device::mojom::SerialPortInfoPtr> ports_;

  base::WeakPtrFactory<SerialChooserController> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SerialChooserController);
};

#endif  // CHROME_BROWSER_UI_SERIAL_SERIAL_CHOOSER_CONTROLLER_H_
