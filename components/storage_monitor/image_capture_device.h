// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STORAGE_MONITOR_IMAGE_CAPTURE_DEVICE_H_
#define COMPONENTS_STORAGE_MONITOR_IMAGE_CAPTURE_DEVICE_H_

#import <Foundation/Foundation.h>
#import <ImageCaptureCore/ImageCaptureCore.h>

#include "base/apple/foundation_util.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/lock.h"

namespace storage_monitor {

// Clients use this listener interface to get notifications about
// events happening as a particular ImageCapture device is interacted with.
// Clients drive the interaction through the ImageCaptureDeviceManager
// and the ImageCaptureDevice classes, and get notifications of
// events through this interface.
class ImageCaptureDeviceListener {
 public:
  virtual ~ImageCaptureDeviceListener() = default;

  // Get a notification that a particular item has been found on the device.
  // These calls will come automatically after a new device is initialized.
  // Names are in relative path form, so subdirectories and files in them will
  // be passed as "dir/subdir/filename". These same relative filenames should
  // be used as keys to download files.
  virtual void ItemAdded(const std::string& name,
                         const base::File::Info& info) = 0;

  // Called when there are no more items to retrieve.
  virtual void NoMoreItems() = 0;

  // Called upon completion of a file download request.
  // Note: in NOT_FOUND error case, may be called inline with the download
  // request.
  virtual void DownloadedFile(const std::string& name,
                              base::File::Error error) = 0;

  // Called to let the client know the device is removed. The client should
  // set the ImageCaptureDevice listener to null upon receiving this call.
  virtual void DeviceRemoved() = 0;
};

}  // namespace storage_monitor

// Interface to a camera device found by ImageCaptureCore. This class manages a
// session to the camera and provides the backing interactions to present the
// media files on it to the filesystem delegate. FilePaths will be artificial,
// like "/$device_id/" + name.
// Note that all interactions with this class must happen on the UI thread.
@interface ImageCaptureDevice
    : NSObject <ICCameraDeviceDelegate, ICCameraDeviceDownloadDelegate>

- (instancetype)initWithCameraDevice:(ICCameraDevice*)cameraDevice;
- (void)setListener:
        (base::WeakPtr<storage_monitor::ImageCaptureDeviceListener>)listener;
- (void)open;
- (void)close;

- (void)eject;

// Download the given file |name| to the provided |local_path|. Completion
// notice will be sent to the listener's DownloadedFile method. The name
// should be of the same form as those sent to the listener's ItemAdded method.
- (void)downloadFile:(const std::string&)name
           localPath:(const base::FilePath&)localPath;

@end

#endif  // COMPONENTS_STORAGE_MONITOR_IMAGE_CAPTURE_DEVICE_H_
