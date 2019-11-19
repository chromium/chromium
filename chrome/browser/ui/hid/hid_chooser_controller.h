// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HID_HID_CHOOSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_HID_HID_CHOOSER_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/chooser_controller/chooser_controller.h"
#include "content/public/browser/hid_chooser.h"
#include "services/device/public/mojom/hid.mojom-forward.h"
#include "third_party/blink/public/mojom/hid/hid.mojom.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;
}  // namespace content

class HidChooserContext;

// HidChooserController provides data for the WebHID API permission prompt.
// It is owned by ChooserBubbleDelegate.
class HidChooserController : public ChooserController {
 public:
  // Construct a chooser controller for Human Interface Devices (HID).
  // |render_frame_host| is used to initialize the chooser strings and to access
  // the requesting and embedding origins. |callback| is called when the chooser
  // is closed, either by selecting an item or by dismissing the chooser dialog.
  // The callback is called with the selected device, or nullptr if no device is
  // selected.
  HidChooserController(content::RenderFrameHost* render_frame_host,
                       std::vector<blink::mojom::HidDeviceFilterPtr> filters,
                       content::HidChooser::Callback callback);
  ~HidChooserController() override;

  // ChooserController:
  bool ShouldShowHelpButton() const override;
  base::string16 GetNoOptionsText() const override;
  base::string16 GetOkButtonLabel() const override;
  size_t NumOptions() const override;
  base::string16 GetOption(size_t index) const override;
  bool IsPaired(size_t index) const override;
  void Select(const std::vector<size_t>& indices) override;
  void Cancel() override;
  void Close() override;
  void OpenHelpCenterUrl() const override;

 private:
  void OnGotDevices(std::vector<device::mojom::HidDeviceInfoPtr> devices);
  bool ShouldExcludeDevice(const device::mojom::HidDeviceInfo& device) const;
  bool FilterMatchesAny(const device::mojom::HidDeviceInfo& device) const;

  std::vector<blink::mojom::HidDeviceFilterPtr> filters_;
  content::HidChooser::Callback callback_;
  const url::Origin requesting_origin_;
  const url::Origin embedding_origin_;

  // The lifetime of the chooser context is tied to the browser context used to
  // create it, and may be destroyed while the chooser is still active.
  base::WeakPtr<HidChooserContext> chooser_context_;
  std::vector<device::mojom::HidDeviceInfoPtr> devices_;

  base::WeakPtrFactory<HidChooserController> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HidChooserController);
};

#endif  // CHROME_BROWSER_UI_HID_HID_CHOOSER_CONTROLLER_H_
