// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_CHOOSER_CONTEXT_MOCK_DEVICE_OBSERVER_H_
#define CHROME_BROWSER_USB_USB_CHOOSER_CONTEXT_MOCK_DEVICE_OBSERVER_H_

#include "chrome/browser/usb/usb_chooser_context.h"
#include "services/device/public/mojom/usb_manager.mojom-forward.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockDeviceObserver : public UsbChooserContext::DeviceObserver {
 public:
  MockDeviceObserver();
  ~MockDeviceObserver() override;

  MOCK_METHOD1(OnDeviceAdded, void(const device::mojom::UsbDeviceInfo&));
  MOCK_METHOD1(OnDeviceRemoved, void(const device::mojom::UsbDeviceInfo&));
  MOCK_METHOD0(OnDeviceManagerConnectionError, void());
};

#endif  // CHROME_BROWSER_USB_USB_CHOOSER_CONTEXT_MOCK_DEVICE_OBSERVER_H_
