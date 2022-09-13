// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/storage_monitor/image_capture_device_manager.h"

#include "base/memory/raw_ptr.h"

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
  base::scoped_nsobject<ICDeviceBrowser> _deviceBrowser;
  base::scoped_nsobject<NSMutableArray> _cameras;

  // Guaranteed to outlive this class.
  // TODO(gbillock): Update when ownership chains go up through
  // a StorageMonitor subclass.
  raw_ptr<storage_monitor::StorageMonitor::Receiver> _notifications;
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
    _cameras.reset([[NSMutableArray alloc] init]);
    _notifications = nullptr;

    _deviceBrowser.reset([[ICDeviceBrowser alloc] init]);
    [_deviceBrowser setDelegate:self];
    [_deviceBrowser
        setBrowsedDeviceTypeMask:ICDeviceTypeMask{
                                     ICDeviceTypeMaskCamera |
                                     UInt{ICDeviceLocationTypeMaskLocal}}];
    [_deviceBrowser start];
  }
  return self;
}

- (void)setNotifications:
            (storage_monitor::StorageMonitor::Receiver*)notifications {
  _notifications = notifications;
}

- (void)close {
  [_deviceBrowser setDelegate:nil];
  [_deviceBrowser stop];
  _deviceBrowser.reset();
  _cameras.reset();
}

- (ImageCaptureDevice*) deviceForUUID:(const std::string&)uuid {
  for (ICCameraDevice* camera in _cameras.get()) {
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

  [_cameras addObject:addedDevice];

  // TODO(gbillock): use [cameraDevice mountPoint] here when possible.
  storage_monitor::StorageInfo info(
      storage_monitor::StorageInfo::MakeDeviceId(
          storage_monitor::StorageInfo::MAC_IMAGE_CAPTURE,
          base::SysNSStringToUTF8([cameraDevice UUIDString])),
      std::string(), base::SysNSStringToUTF16([cameraDevice name]),
      std::u16string(), std::u16string(), 0);
  _notifications->ProcessAttach(info);
}

- (void)deviceBrowser:(ICDeviceBrowser*)browser
      didRemoveDevice:(ICDevice*)device
            moreGoing:(BOOL)moreGoing {
  if (!(device.type & ICDeviceTypeCamera))
    return;

  std::string uuid = base::SysNSStringToUTF8([device UUIDString]);

  // May delete |device|.
  [_cameras removeObject:device];

  _notifications->ProcessDetach(storage_monitor::StorageInfo::MakeDeviceId(
      storage_monitor::StorageInfo::MAC_IMAGE_CAPTURE, uuid));
}

- (ICDeviceBrowser*)deviceBrowserForTest {
  return _deviceBrowser.get();
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
    base::OnceCallback<void(StorageMonitor::EjectStatus)> callback) {
  base::scoped_nsobject<ImageCaptureDevice> camera_device(
      [[device_browser_ deviceForUUID:uuid] retain]);
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
  return device_browser_.get();
}

ICDeviceBrowser* ImageCaptureDeviceManager::device_browser_for_test() {
  return [device_browser_ deviceBrowserForTest];
}

}  // namespace storage_monitor
