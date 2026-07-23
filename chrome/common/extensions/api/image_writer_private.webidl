// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The different stages of a write call.
//
// <dl>
//    <dt>confirmation</dt>
//    <dd>The process starts by prompting the user for confirmation.</dd>
//    <dt>download</dt>
//    <dd>The image file is being download if a remote image was
//    requested.</dd>
//    <dt>verifyDownload</dt>
//    <dd>The download is being verified to match the image hash, if
//    provided</dd>
//    <dt>unzip</dt>
//    <dd>The image is being extracted from the downloaded zip file</dd>
//    <dt>write</dt>
//    <dd>The image is being written to disk.</dd>
//    <dt>verifyWrite</dt>
//    <dt>The system is verifying that the written image matches the
//    downloaded image.</dd>
// <dl>
enum Stage {
  "confirmation",
  "download",
  "verifyDownload",
  "unzip",
  "write",
  "verifyWrite",
  "unknown"
};

// Options for writing an image.
dictionary UrlWriteOptions {
  // If present, verify that the downloaded image matches this hash.
  DOMString imageHash;
  // If true, save the downloaded image as a file using the user's downloads
  // preferences.
  boolean saveAsDownload;
};

dictionary ProgressInfo {
  // The $(ref:Stage) that the write process is currently in.
  required Stage stage;
  // Current progress within the stage.
  required long percentComplete;
};

dictionary RemovableStorageDevice {
  required DOMString storageUnitId;
  required double capacity;
  required DOMString vendor;
  required DOMString model;
  required boolean removable;
};

callback OnWriteProgressListener = undefined (ProgressInfo info);

interface OnWriteProgressEvent : ExtensionEvent {
  static undefined addListener(OnWriteProgressListener listener);
  static undefined removeListener(OnWriteProgressListener listener);
  static boolean hasListener(OnWriteProgressListener listener);
};

callback OnWriteCompleteListener = undefined ();

interface OnWriteCompleteEvent : ExtensionEvent {
  static undefined addListener(OnWriteCompleteListener listener);
  static undefined removeListener(OnWriteCompleteListener listener);
  static boolean hasListener(OnWriteCompleteListener listener);
};

callback OnWriteErrorListener = undefined (ProgressInfo info, DOMString error);

interface OnWriteErrorEvent : ExtensionEvent {
  static undefined addListener(OnWriteErrorListener listener);
  static undefined removeListener(OnWriteErrorListener listener);
  static boolean hasListener(OnWriteErrorListener listener);
};

callback OnDeviceInsertedListener = undefined (RemovableStorageDevice device);

interface OnDeviceInsertedEvent : ExtensionEvent {
  static undefined addListener(OnDeviceInsertedListener listener);
  static undefined removeListener(OnDeviceInsertedListener listener);
  static boolean hasListener(OnDeviceInsertedListener listener);
};

callback OnDeviceRemovedListener = undefined (RemovableStorageDevice device);

interface OnDeviceRemovedEvent : ExtensionEvent {
  static undefined addListener(OnDeviceRemovedListener listener);
  static undefined removeListener(OnDeviceRemovedListener listener);
  static boolean hasListener(OnDeviceRemovedListener listener);
};

[instanceOf=FileEntry]
typedef object FileEntry;

// Use the <code>chrome.image_writer</code> API to write images to removable
// media.
interface ImageWriterPrivate {
  // Write an image to the disk downloaded from the provided URL.  The callback
  // will be called when the entire operation completes, either successfully or
  // on error.
  //
  // |storageUnitId|: The identifier for the storage unit
  // |imageUrl|: The url of the image to download which will be written to the
  // storage unit identified by |storageUnitId|
  // |options|: Optional parameters if comparing the download with a given hash
  // or saving the download to the users Downloads folder instead of a temporary
  // directory is desired
  // |Returns|: The callback which signifies that the write operation has been
  // started by the system and provides a unique ID for this operation.
  [requiredCallback] static Promise<undefined> writeFromUrl(
      DOMString storageUnitId, DOMString imageUrl,
      optional UrlWriteOptions options);

  // Write an image to the disk, prompting the user to supply the image from a
  // local file.  The callback will be called when the entire operation
  // completes, either successfully or on error.
  //
  // |storageUnitId|: The identifier for the storage unit
  // |fileEntry|: The FileEntry object of the image to be burned.
  // |Returns|: The callback which signifies that the write operation has been
  // started by the system and provides a unique ID for this operation.
  [requiredCallback] static Promise<undefined> writeFromFile(
      DOMString storageUnitId, FileEntry fileEntry);

  // Cancel a current write operation.
  //
  // |Returns|: The callback which is triggered with the write is successfully
  // cancelled, passing the $(ref:ProgressInfo) of the operation at the time it
  // was cancelled.
  [requiredCallback] static Promise<undefined> cancelWrite();

  // Destroys the partition table of a disk, effectively erasing it.  This is a
  // fairly quick operation and so it does not have complex stages or progress
  // information, just a write phase.
  //
  // |storageUnitId|: The identifier of the storage unit to wipe
  // |Returns|: A callback that triggers when the operation has been
  // successfully started.
  [requiredCallback] static Promise<undefined> destroyPartitions(
      DOMString storageUnitId);

  // List all the removable block devices currently attached to the system.
  // |Returns|: A callback called with a list of removable storage devices
  // |PromiseValue|: devices
  [requiredCallback] static Promise<sequence<RemovableStorageDevice>>
      listRemovableStorageDevices();

  // Fires periodically throughout the writing operation and at least once per
  // stage.
  static attribute OnWriteProgressEvent onWriteProgress;

  // Fires when the write operation has completely finished, such as all devices
  // being finalized and resources released.
  static attribute OnWriteCompleteEvent onWriteComplete;

  // Fires when an error occured during writing, passing the $(ref:ProgressInfo)
  // of the operation at the time the error occured.
  static attribute OnWriteErrorEvent onWriteError;

  // Fires when a removable storage device is inserted.
  static attribute OnDeviceInsertedEvent onDeviceInserted;

  // Fires when a removable storage device is removed.
  static attribute OnDeviceRemovedEvent onDeviceRemoved;
};

partial interface Browser {
  static attribute ImageWriterPrivate imageWriterPrivate;
};
