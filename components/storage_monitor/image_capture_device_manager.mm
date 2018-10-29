// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/storage_monitor/image_capture_device_manager.h"

#import <ImageCaptureCore/ImageCaptureCore.h>

#import "components/storage_monitor/image_capture_device.h"
#include "components/storage_monitor/storage_info.h"

namespace {

storage_monitor::ImageCaptureDeviceManager* g_image_capture_device_manager =
    NULL;

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
  base::scoped_nsobject<ICDeviceBrowser> deviceBrowser_;
  base::scoped_nsobject<NSMutableArray> cameras_;

  // Guaranteed to outlive this class.
  // TODO(gbillock): Update when ownership chains go up through
  // a StorageMonitor subclass.
  storage_monitor::StorageMonitor::Receiver* notifications_;
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
    cameras_.reset([[NSMutableArray alloc] init]);
    notifications_ = NULL;

    deviceBrowser_.reset([[ICDeviceBrowser alloc] init]);
    [deviceBrowser_ setDelegate:self];
    [deviceBrowser_ setBrowsedDeviceTypeMask:
        static_cast<ICDeviceTypeMask>(
            ICDeviceTypeMaskCamera | ICDeviceLocationTypeMaskLocal)];
    [deviceBrowser_ start];
  }
  return self;
}

- (void)setNotifications:
            (storage_monitor::StorageMonitor::Receiver*)notifications {
  notifications_ = notifications;
}

- (void)close {
  [deviceBrowser_ setDelegate:nil];
  [deviceBrowser_ stop];
  deviceBrowser_.reset();
  cameras_.reset();
}

- (ImageCaptureDevice*) deviceForUUID:(const std::string&)uuid {
  for (ICCameraDevice* camera in cameras_.get()) {
    NSString* camera_id = [camera UUIDString];
    if (base::SysNSStringToUTF8(camera_id) == uuid) {
      return [[[ImageCaptureDevice alloc]
          initWithCameraDevice:camera] autorelease];
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
      base::mac::ObjCCastStrict<ICCameraDevice>(addedDevice);

  [cameras_ addObject:addedDevice];

  // TODO(gbillock): use [cameraDevice mountPoint] here when possible.
  storage_monitor::StorageInfo info(
      storage_monitor::StorageInfo::MakeDeviceId(
          storage_monitor::StorageInfo::MAC_IMAGE_CAPTURE,
          base::SysNSStringToUTF8([cameraDevice UUIDString])),
      std::string(),
      base::SysNSStringToUTF16([cameraDevice name]),
      base::string16(),
      base::string16(),
      0);
  notifications_->ProcessAttach(info);
}

- (void)deviceBrowser:(ICDeviceBrowser*)browser
      didRemoveDevice:(ICDevice*)device
            moreGoing:(BOOL)moreGoing {
  if (!(device.type & ICDeviceTypeCamera))
    return;

  std::string uuid = base::SysNSStringToUTF8([device UUIDString]);

  // May delete |device|.
  [cameras_ removeObject:device];

  notifications_->ProcessDetach(storage_monitor::StorageInfo::MakeDeviceId(
      storage_monitor::StorageInfo::MAC_IMAGE_CAPTURE, uuid));
}

- (ICDeviceBrowser*)deviceBrowserForTest {
  return deviceBrowser_.get();
}

@end  // ImageCaptureDeviceManagerImpl

namespace storage_monitor {

ImageCaptureDeviceManager::ImageCaptureDeviceManager() {
  device_browser_.reset([[ImageCaptureDeviceManagerImpl alloc] init]);
  g_image_capture_device_manager = this;
}

ImageCaptureDeviceManager::~ImageCaptureDeviceManager() {
  g_image_capture_device_manager = NULL;
  [device_browser_ close];
}

void ImageCaptureDeviceManager::SetNotifications(
    StorageMonitor::Receiver* notifications) {
  [device_browser_ setNotifications:notifications];
}

void ImageCaptureDeviceManager::EjectDevice(
    const std::string& uuid,
    base::Callback<void(StorageMonitor::EjectStatus)> callback) {
  base::scoped_nsobject<ImageCaptureDevice> camera_device(
      [[device_browser_ deviceForUUID:uuid] retain]);
  [camera_device eject];
  [camera_device close];
  callback.Run(StorageMonitor::EJECT_OK);
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
  return device_browser_.get();
}

ICDeviceBrowser* ImageCaptureDeviceManager::device_browser_for_test() {
  return [device_browser_ deviceBrowserForTest];
}

}  // namespace storage_monitor
