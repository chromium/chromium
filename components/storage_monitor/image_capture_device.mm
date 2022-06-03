// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/storage_monitor/image_capture_device.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_thread.h"

namespace storage_monitor {

namespace {

base::File::Error RenameFile(const base::FilePath& downloaded_filename,
                             const base::FilePath& desired_filename) {
  bool success = base::ReplaceFile(downloaded_filename, desired_filename, NULL);
  return success ? base::File::FILE_OK : base::File::FILE_ERROR_NOT_FOUND;
}

void ReturnRenameResultToListener(
    base::WeakPtr<ImageCaptureDeviceListener> listener,
    const std::string& name,
    const base::File::Error& result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (listener)
    listener->DownloadedFile(name, result);
}

base::Time NSDateToBaseTime(NSDate* date) {
  return base::Time::FromDoubleT([date timeIntervalSince1970]);
}

base::FilePath PathForCameraItem(ICCameraItem* item) {
  std::string name = base::SysNSStringToUTF8([item name]);

  std::vector<std::string> components;
  ICCameraFolder* folder = [item parentFolder];
  while (folder != nil) {
    components.push_back(base::SysNSStringToUTF8([folder name]));
    folder = [folder parentFolder];
  }
  base::FilePath path;
  for (std::vector<std::string>::reverse_iterator i = components.rbegin();
       i != components.rend(); ++i) {
    path = path.Append(*i);
  }
  path = path.Append(name);

  return path;
}

}  // namespace

}  // namespace storage_monitor

@implementation ImageCaptureDevice

- (instancetype)initWithCameraDevice:(ICCameraDevice*)cameraDevice {
  if ((self = [super init])) {
    _camera.reset([cameraDevice retain]);
    [_camera setDelegate:self];
    _closing = false;
  }
  return self;
}

- (void)dealloc {
  // Make sure the session was closed and listener set to null
  // before destruction.
  DCHECK(![_camera delegate]);
  DCHECK(!_listener);
  [super dealloc];
}

- (void)setListener:(base::WeakPtr<storage_monitor::ImageCaptureDeviceListener>)
        listener {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  _listener = listener;
}

- (void)open {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(_listener);
  [_camera requestOpenSession];
}

- (void)close {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  _closing = true;
  [_camera cancelDownload];
  [_camera requestCloseSession];
  [_camera setDelegate:nil];
  _listener.reset();
}

- (void)eject {
  [_camera requestEjectOrDisconnect];
}

- (void)downloadFile:(const std::string&)name
           localPath:(const base::FilePath&)localPath {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Find the file with that name and start download.
  for (ICCameraItem* item in [_camera mediaFiles]) {
    std::string itemName = storage_monitor::PathForCameraItem(item).value();
    if (itemName == name) {
      // To create save options for ImageCapture, we need to
      // split the target filename into directory/name
      // and encode the directory as a URL.
      NSString* saveDirectory =
          base::mac::FilePathToNSString(localPath.DirName());
      NSString* saveFilename =
          base::mac::FilePathToNSString(localPath.BaseName());

      NSMutableDictionary* options =
          [NSMutableDictionary dictionaryWithCapacity:3];
      options[ICDownloadsDirectoryURL] =
          [NSURL fileURLWithPath:saveDirectory isDirectory:YES];
      options[ICSaveAsFilename] = saveFilename;
      options[ICOverwrite] = @YES;

      [_camera requestDownloadFile:base::mac::ObjCCastStrict<ICCameraFile>(item)
                           options:options
                  downloadDelegate:self
               didDownloadSelector:
                   @selector(didDownloadFile:error:options:contextInfo:)
                       contextInfo:NULL];
      return;
    }
  }

  if (_listener)
    _listener->DownloadedFile(name, base::File::FILE_ERROR_NOT_FOUND);
}

- (void)cameraDevice:(ICCameraDevice*)camera didAddItem:(ICCameraItem*)item {
  base::File::Info info;
  if ([[item UTI] isEqualToString:base::mac::CFToNSCast(kUTTypeFolder)])
    info.is_directory = true;
  else
    info.size = [base::mac::ObjCCastStrict<ICCameraFile>(item) fileSize];

  base::FilePath path = storage_monitor::PathForCameraItem(item);

  info.last_modified =
      storage_monitor::NSDateToBaseTime([item modificationDate]);
  info.creation_time = storage_monitor::NSDateToBaseTime([item creationDate]);
  info.last_accessed = info.last_modified;

  if (_listener)
    _listener->ItemAdded(path.value(), info);
}

- (void)cameraDevice:(ICCameraDevice*)camera didAddItems:(NSArray*)items {
  for (ICCameraItem* item in items)
    [self cameraDevice:camera didAddItem:item];
}

- (void)didRemoveDevice:(ICDevice*)device {
  device.delegate = NULL;
  if (_listener)
    _listener->DeviceRemoved();
}

// Notifies that a session was opened with the given device; potentially
// with an error.
- (void)device:(ICDevice*)device didOpenSessionWithError:(NSError*)error {
  if (error)
    [self didRemoveDevice:_camera];
}

- (void)device:(ICDevice*)device didEncounterError:(NSError*)error {
  if (error && _listener)
    _listener->DeviceRemoved();
}

// When this message is received, all media metadata is now loaded.
- (void)deviceDidBecomeReadyWithCompleteContentCatalog:(ICDevice*)device {
  if (_listener)
    _listener->NoMoreItems();
}

- (void)didDownloadFile:(ICCameraFile*)file
                  error:(NSError*)error
                options:(NSDictionary*)options
            contextInfo:(void*)contextInfo {
  if (_closing)
    return;

  std::string name = storage_monitor::PathForCameraItem(file).value();

  if (error) {
    DVLOG(1) << "error..."
             << base::SysNSStringToUTF8([error localizedDescription]);
    if (_listener)
      _listener->DownloadedFile(name, base::File::FILE_ERROR_FAILED);
    return;
  }

  std::string savedFilename = base::SysNSStringToUTF8(options[ICSavedFilename]);
  std::string saveAsFilename =
      base::SysNSStringToUTF8(options[ICSaveAsFilename]);
  if (savedFilename == saveAsFilename) {
    if (_listener)
      _listener->DownloadedFile(name, base::File::FILE_OK);
    return;
  }

  // ImageCapture did not save the file into the name we gave it in the
  // options. It picks a new name according to its best lights, so we need
  // to rename the file.
  base::FilePath saveDir(
      base::SysNSStringToUTF8([options[ICDownloadsDirectoryURL] path]));
  base::FilePath saveAsPath = saveDir.Append(saveAsFilename);
  base::FilePath savedPath = saveDir.Append(savedFilename);
  // Shared result value from file-copy closure to tell-listener closure.
  // This is worth blocking shutdown, as otherwise a file that has been
  // downloaded will be incorrectly named.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&storage_monitor::RenameFile, savedPath, saveAsPath),
      base::BindOnce(&storage_monitor::ReturnRenameResultToListener, _listener,
                     name));
}

// MacOS 10.14 SDK methods, not yet implemented (https://crbug.com/849689)
- (void)cameraDevice:(ICCameraDevice*)camera
      didRenameItems:(NSArray<ICCameraItem*>*)items {
  NOTIMPLEMENTED();
}

- (void)cameraDevice:(ICCameraDevice*)camera didRemoveItem:(ICCameraItem*)item {
  NOTIMPLEMENTED();
}

- (void)cameraDevice:(ICCameraDevice*)camera
    didCompleteDeleteFilesWithError:(NSError*)error {
  NOTIMPLEMENTED();
}

- (void)cameraDeviceDidChangeCapability:(ICCameraDevice*)camera {
  NOTIMPLEMENTED();
}

- (void)cameraDevice:(ICCameraDevice*)camera
    didReceiveThumbnailForItem:(ICCameraItem*)item {
  NOTIMPLEMENTED();
}

- (void)cameraDevice:(ICCameraDevice*)camera
    didReceiveMetadataForItem:(ICCameraItem*)item {
  NOTIMPLEMENTED();
}

- (void)cameraDevice:(ICCameraDevice*)camera
    didReceivePTPEvent:(NSData*)eventData {
  NOTIMPLEMENTED();
}

// Mac 10.15 SDK methods, not yet implemented (https://crbug.com/849689)

- (void)cameraDevice:(ICCameraDevice*)camera didRemoveItems:(NSArray*)items {
  NOTIMPLEMENTED();
}

- (void)cameraDevice:(ICCameraDevice*)camera
    didReceiveThumbnail:(CGImageRef)thumbnail
                forItem:(ICCameraItem*)item
                  error:(NSError*)error {
  NOTIMPLEMENTED();
}

- (void)cameraDevice:(ICCameraDevice*)camera
    didReceiveMetadata:(NSDictionary*)metadata
               forItem:(ICCameraItem*)item
                 error:(NSError*)error {
  NOTIMPLEMENTED();
}

- (void)cameraDeviceDidEnableAccessRestriction:(ICDevice*)device {
  NOTIMPLEMENTED();
}

- (void)cameraDeviceDidRemoveAccessRestriction:(ICDevice*)device {
  NOTIMPLEMENTED();
}

- (void)device:(ICDevice*)device didCloseSessionWithError:(NSError*)error {
  NOTIMPLEMENTED();
}

@end  // ImageCaptureDevice
