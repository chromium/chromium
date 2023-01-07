// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_WEB_USB_DETECTOR_H_
#define CHROME_BROWSER_USB_WEB_USB_DETECTOR_H_

#include <map>

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "services/device/public/mojom/usb_manager_client.mojom.h"
#include "url/gurl.h"

class WebUsbDetector : public device::mojom::UsbDeviceManagerClient {
 public:
  WebUsbDetector();
  WebUsbDetector(const WebUsbDetector&) = delete;
  WebUsbDetector& operator=(const WebUsbDetector&) = delete;
  ~WebUsbDetector() override;

  // Initializes the WebUsbDetector.
  void Initialize();

  void SetDeviceManagerForTesting(
      mojo::PendingRemote<device::mojom::UsbDeviceManager> fake_device_manager);
  void RemoveNotification(const std::string& id);

 private:
  // device::mojom::UsbDeviceManagerClient implementation.
  void OnDeviceAdded(device::mojom::UsbDeviceInfoPtr device_info) override;
  void OnDeviceRemoved(device::mojom::UsbDeviceInfoPtr device_info) override;

  bool IsDisplayingNotification(const GURL& url);

  std::map<std::string, GURL> open_notifications_by_id_;

  // Connection to |device_manager_instance_|.
  mojo::Remote<device::mojom::UsbDeviceManager> device_manager_;
  mojo::AssociatedReceiver<device::mojom::UsbDeviceManagerClient>
      client_receiver_{this};

  base::WeakPtrFactory<WebUsbDetector> weak_factory_{this};
};

#endif  // CHROME_BROWSER_USB_WEB_USB_DETECTOR_H_
