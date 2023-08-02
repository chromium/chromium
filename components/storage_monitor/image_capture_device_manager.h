// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STORAGE_MONITOR_IMAGE_CAPTURE_DEVICE_MANAGER_H_
#define COMPONENTS_STORAGE_MONITOR_IMAGE_CAPTURE_DEVICE_MANAGER_H_

#import <Foundation/Foundation.h>

#include <string>

#include "components/storage_monitor/storage_monitor.h"

class MTPDeviceDelegateImplMacTest;

@protocol ICDeviceBrowserDelegate;
@class ICDeviceBrowser;
@class ImageCaptureDevice;
@class ImageCaptureDeviceManagerImpl;

namespace storage_monitor {

// Upon creation, begins monitoring for any attached devices using the
// ImageCapture API. Notifies clients of the presence of such devices
// (i.e. cameras,  USB cards) using the SystemMonitor and makes them
// available using |deviceForUUID|.
class ImageCaptureDeviceManager {
 public:
  ImageCaptureDeviceManager();
  ~ImageCaptureDeviceManager();

  // The UUIDs passed here are available in the device attach notifications
  // given through SystemMonitor. They're gotten by cracking the device ID
  // and taking the unique ID output.
  static ImageCaptureDevice* deviceForUUID(const std::string& uuid);

  // Returns a weak pointer to the internal ImageCapture interface protocol.
  id<ICDeviceBrowserDelegate> device_browser_delegate();

  // Sets the receiver for device attach/detach notifications.
  // TODO(gbillock): Move this to be a constructor argument.
  void SetNotifications(StorageMonitor::Receiver* notifications);

  // Eject the given device. The ID passed is not the device ID, but the
  // ImageCapture UUID.
  void EjectDevice(
      const std::string& uuid,
      base::OnceCallback<void(StorageMonitor::EjectStatus)> callback);

 private:
  ImageCaptureDeviceManagerImpl* __strong device_browser_;

  // Returns a weak pointer to the internal device browser.
  ICDeviceBrowser* device_browser_for_test();
  friend class ImageCaptureDeviceManagerTest;
  friend class ::MTPDeviceDelegateImplMacTest;
};

}  // namespace storage_monitor

#endif  // COMPONENTS_STORAGE_MONITOR_IMAGE_CAPTURE_DEVICE_MANAGER_H_
