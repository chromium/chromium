// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '../chrome_util.js';
import {WaitableEvent} from '../waitable_event.js';

import {ChromeDirectoryEntry} from './chrome_file_system_entry.js';
import {Filenamer, IMAGE_PREFIX, VIDEO_PREFIX} from './file_namer.js';
import {
  AbstractDirectoryEntry,  // eslint-disable-line no-unused-vars
  AbstractFileEntry,       // eslint-disable-line no-unused-vars
} from './file_system_entry.js';
import * as idb from './idb.js';
import {getMaybeLazyDirectory} from './lazy_directory_entry.js';
import {NativeDirectoryEntry} from './native_file_system_entry.js';

/**
 * Checks if the entry's name has the video prefix.
 * @param {!AbstractFileEntry} entry File entry.
 * @return {boolean} Has the video prefix or not.
 */
export function hasVideoPrefix(entry) {
  return entry.name.startsWith(VIDEO_PREFIX);
}

/**
 * Checks if the entry's name has the image prefix.
 * @param {!AbstractFileEntry} entry File entry.
 * @return {boolean} Has the image prefix or not.
 */
function hasImagePrefix(entry) {
  return entry.name.startsWith(IMAGE_PREFIX);
}

/**
 * Temporary directory in the internal file system.
 * @type {?AbstractDirectoryEntry}
 */
let internalTempDir = null;

/**
 * Camera directory in the external file system.
 * @type {?AbstractDirectoryEntry}
 */
let cameraDir = null;

/**
 * Gets camera directory used by CCA.
 * @return {?AbstractDirectoryEntry}
 */
export function getCameraDirectory() {
  return cameraDir;
}

/**
 * Initializes the temporary directory in the internal file system.
 * @return {!Promise<!AbstractDirectoryEntry>} Promise for the directory result.
 */
function initInternalTempDir() {
  return new Promise((resolve, reject) => {
    webkitRequestFileSystem(
        window.TEMPORARY, 768 * 1024 * 1024 /* 768MB */,
        (fs) => resolve(new ChromeDirectoryEntry(fs.root)), reject);
  });
}

/**
 * Initializes the camera directory in the external file system.
 * @return {!Promise<?AbstractDirectoryEntry>} Promise for the directory result.
 */
async function initCameraDirectory() {
  const handle = new WaitableEvent();

  // We use the sessionStorage to decide if we should use the handle in the
  // database or the handle from the launch queue so that we can use the new
  // handle if the handle changes in the future.
  const isConsumedHandle = window.sessionStorage.getItem('IsConsumedHandle');
  if (isConsumedHandle !== null) {
    const storedHandle = await idb.get(idb.KEY_CAMERA_DIRECTORY_HANDLE);
    handle.signal(storedHandle);
  } else {
    const launchQueue = window.launchQueue;
    assert(launchQueue !== undefined);
    launchQueue.setConsumer(async (launchParams) => {
      assert(launchParams.files.length > 0);
      const dir =
          /** @type {!FileSystemDirectoryHandle} */ (launchParams.files[0]);
      assert(dir.kind === 'directory');

      await idb.set(idb.KEY_CAMERA_DIRECTORY_HANDLE, dir);
      window.sessionStorage.setItem('IsConsumedHandle', 'true');

      handle.signal(dir);
    });
  }
  const dir = await handle.wait();
  const myFilesDir = new NativeDirectoryEntry(dir);
  return getMaybeLazyDirectory(myFilesDir, 'Camera');
}

/**
 * Initializes file systems. This function should be called only once in the
 * beginning of the app.
 * @return {!Promise}
 */
export async function initialize() {
  internalTempDir = await initInternalTempDir();
  assert(internalTempDir !== null);

  cameraDir = await initCameraDirectory();
  assert(cameraDir !== null);
}

/**
 * Saves photo blob or metadata blob into predefined default location.
 * @param {!Blob} blob Data of the photo to be saved.
 * @param {string} name Filename of the photo to be saved.
 * @return {!Promise<?AbstractFileEntry>} Promise for the result.
 */
export async function saveBlob(blob, name) {
  const file = await cameraDir.createFile(name);
  assert(file !== null);

  await file.write(blob);
  return file;
}

/**
 * Creates a file for saving video recording result.
 * @return {!Promise<!AbstractFileEntry>} Newly created video file.
 * @throws {!Error} If failed to create video file.
 */
export async function createVideoFile() {
  const name = new Filenamer().newVideoName();
  const file = await cameraDir.createFile(name);
  if (file === null) {
    throw new Error('Failed to create video temp file.');
  }
  return file;
}

/**
 * @type {string}
 */
const PRIVATE_TEMPFILE_NAME = 'video-intent.mkv';

/**
 * @return {!Promise<!AbstractFileEntry>} Newly created temporary file.
 * @throws {!Error} If failed to create video temp file.
 */
export async function createPrivateTempVideoFile() {
  // TODO(inker): Handles running out of space case.
  const dir = internalTempDir;
  assert(dir !== null);
  const file = await dir.createFile(PRIVATE_TEMPFILE_NAME);
  if (file === null) {
    throw new Error('Failed to create private video temp file.');
  }
  return file;
}

/**
 * Gets the picture entries.
 * @return {!Promise<!Array<!AbstractFileEntry>>} Promise for the picture
 *     entries.
 */
export async function getEntries() {
  const entries = await cameraDir.getFiles();
  return entries.filter((entry) => {
    if (!hasVideoPrefix(entry) && !hasImagePrefix(entry)) {
      return false;
    }
    return entry.name.match(/_(\d{8})_(\d{6})(?: \((\d+)\))?/);
  });
}

/**
 * Returns an URL for a picture given by the file |entry|.
 * @param {!AbstractFileEntry} entry The file entry of the picture.
 * @return {!Promise<string>} Promise for the result.
 */
export async function pictureURL(entry) {
  const file = await entry.file();
  return URL.createObjectURL(file);
}
