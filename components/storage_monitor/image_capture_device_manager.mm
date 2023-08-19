// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/storage_monitor/image_capture_device_manager.h"

#import <ImageCaptureCore/ImageCaptureCore.h>

#include "base/memory/raw_ptr.h"
#import "components/storage_monitor/image_capture_device.h"
#include "components/storage_monitor/storage_info.h"

namespace {

storage_monitor::ImageCaptureDeviceManager* g_image_capture_device_manager =
    nullptr;

}  // namespace

// This class is the surface for the Mac ICDeviceBrowser ImageCaptureCore API.
// Owned by the ChromeBrowserParts and has browser process lifetime. Upon
// creation, it gets a list of attached media volumes (asynchronously) which
// it will eventually forward to StorageMonitor. It will also
// set up an ImageCaptureCore listener to be told when new devices/volumes
// are discovered and existing ones are removed.
@interface ImageCaptureDeviceManagerImpl
    : NSObject<ICDeviceBrowserDelegate> {
 @private
  ICDeviceBrowser* __strong _deviceBrowser;
  NSMutableArray* __strong _cameras;

  // Guaranteed to outlive this class.
  // TODO(gbillock): Update when ownership chains go up through
  // a StorageMonitor subclass.
  raw_ptr<storage_monitor::StorageMonitor::Receiver, DanglingUntriaged>
      _notifications;
}

- (void)setNotifications:
        (storage_monitor::StorageMonitor::Receiver*)notifications;
- (void)close;

// The UUIDs passed here are available in the device attach notifications.
// They're gotten by cracking the device ID and taking the unique ID output.
- (ImageCaptureDevice*)deviceForUUID:(const std::string&)uuid;

- (ICDeviceBrowser*)deviceBrowserForTest;

@end

@implementation ImageCaptureDeviceManagerImpl

- (instancetype)init {
  if ((self = [super init])) {
    _cameras = [[NSMutableArray alloc] init];

    _deviceBrowser = [[ICDeviceBrowser alloc] init];
    _deviceBrowser.delegate = self;
    _deviceBrowser.browsedDeviceTypeMask = ICDeviceTypeMask{
        ICDeviceTypeMaskCamera | UInt{ICDeviceLocationTypeMaskLocal}};
    [_deviceBrowser start];
  }
  return self;
}

- (void)setNotifications:
            (storage_monitor::StorageMonitor::Receiver*)notifications {
  _notifications = notifications;
}

- (void)close {
  _deviceBrowser.delegate = nil;
  [_deviceBrowser stop];
  _deviceBrowser = nil;
  _cameras = nil;
}

- (ImageCaptureDevice*)deviceForUUID:(const std::string&)uuid {
  for (ICCameraDevice* camera in _cameras) {
    if (base::SysNSStringToUTF8(camera.UUIDString) == uuid) {
      return [[ImageCaptureDevice alloc] initWithCameraDevice:camera];
    }
  }
  return nil;
}

- (void)deviceBrowser:(ICDeviceBrowser*)browser
         didAddDevice:(ICDevice*)addedDevice
           moreComing:(BOOL)moreComing {
  if (!(addedDevice.type & ICDeviceTypeCamera))
    return;

  // Ignore mass storage attaches -- those will be handled
  // by Mac's removable storage watcher.
  if ([addedDevice.transportType isEqualToString:ICTransportTypeMassStorage])
    return;

  ICCameraDevice* cameraDevice =
      base::apple::ObjCCastStrict<ICCameraDevice>(addedDevice);

  [_cameras addObject:addedDevice];

  // TODO(gbillock): use cameraDevice.mountPoint here when possible.
  storage_monitor::StorageInfo info(
      storage_monitor::StorageInfo::MakeDeviceId(
          storage_monitor::StorageInfo::MAC_IMAGE_CAPTURE,
          base::SysNSStringToUTF8(cameraDevice.UUIDString)),
      /*device_location=*/std::string(),
      base::SysNSStringToUTF16(cameraDevice.name),
      /*vendor=*/std::u16string(), /*model=*/std::u16string(),
      /*size_in_bytes=*/0);
  _notifications->ProcessAttach(info);
}

- (void)deviceBrowser:(ICDeviceBrowser*)browser
      didRemoveDevice:(ICDevice*)device
            moreGoing:(BOOL)moreGoing {
  if (!(device.type & ICDeviceTypeCamera))
    return;

  std::string uuid = base::SysNSStringToUTF8(device.UUIDString);

  // May delete |device|.
  [_cameras removeObject:device];

  _notifications->ProcessDetach(storage_monitor::StorageInfo::MakeDeviceId(
      storage_monitor::StorageInfo::MAC_IMAGE_CAPTURE, uuid));
}

- (ICDeviceBrowser*)deviceBrowserForTest {
  return _deviceBrowser;
}

@end  // ImageCaptureDeviceManagerImpl

namespace storage_monitor {

ImageCaptureDeviceManager::ImageCaptureDeviceManager() {
  device_browser_ = [[ImageCaptureDeviceManagerImpl alloc] init];
  g_image_capture_device_manager = this;
}

ImageCaptureDeviceManager::~ImageCaptureDeviceManager() {
  g_image_capture_device_manager = nullptr;
  [device_browser_ close];
}

void ImageCaptureDeviceManager::SetNotifications(
    StorageMonitor::Receiver* notifications) {
  [device_browser_ setNotifications:notifications];
}

void ImageCaptureDeviceManager::EjectDevice(
    const std::string& uuid,
    base::OnceCallback<void(StorageMonitor::EjectStatus)> callback) {
  ImageCaptureDevice* camera_device = [device_browser_ deviceForUUID:uuid];
  [camera_device eject];
  [camera_device close];
  std::move(callback).Run(StorageMonitor::EJECT_OK);
}

// static
ImageCaptureDevice* ImageCaptureDeviceManager::deviceForUUID(
    const std::string& uuid) {
  ImageCaptureDeviceManagerImpl* manager =
      g_image_capture_device_manager->device_browser_;
  return [manager deviceForUUID:uuid];
}

id<ICDeviceBrowserDelegate>
ImageCaptureDeviceManager::device_browser_delegate() {
  return device_browser_;
}

ICDeviceBrowser* ImageCaptureDeviceManager::device_browser_for_test() {
  return [device_browser_ deviceBrowserForTest];
}

}  // namespace storage_monitor
