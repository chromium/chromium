// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxy} from '../browser_proxy/browser_proxy.js';
import {assert} from '../chrome_util.js';
import {ChromeDirectoryEntry} from './chrome_file_system_entry.js';
import {Filenamer, IMAGE_PREFIX, VIDEO_PREFIX} from './file_namer.js';
import {
  AbstractDirectoryEntry,  // eslint-disable-line no-unused-vars
  AbstractFileEntry,       // eslint-disable-line no-unused-vars
} from './file_system_entry.js';

/**
 * The prefix of thumbnail files.
 * @type {string}
 */
const THUMBNAIL_PREFIX = 'thumb-';

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
 * Directory in the internal file system.
 * @type {?AbstractDirectoryEntry}
 */
let internalDir = null;

/**
 * Temporary directory in the internal file system.
 * @type {?AbstractDirectoryEntry}
 */
let internalTempDir = null;

/**
 * Directory in the external file system.
 * @type {?AbstractDirectoryEntry}
 */
let externalDir = null;

/**
 * Gets global external directory used by CCA.
 * @return {?AbstractDirectoryEntry}
 */
export function getExternalDirectory() {
  return externalDir;
}

/**
 * Initializes the directory in the internal file system.
 * @return {!Promise<!AbstractDirectoryEntry>} Promise for the directory result.
 */
function initInternalDir() {
  return new Promise((resolve, reject) => {
    webkitRequestFileSystem(
        window.PERSISTENT, 768 * 1024 * 1024 /* 768MB */,
        (fs) => resolve(new ChromeDirectoryEntry(fs.root)), reject);
  });
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
 * Initializes the directory in the external file system.
 * @return {!Promise<?AbstractDirectoryEntry>} Promise for the directory result.
 */
async function initExternalDir() {
  return browserProxy.getExternalDir();
}

/**
 * Regulates the picture name to the desired format if it's in legacy formats.
 * @param {!AbstractFileEntry} entry Picture entry whose name to be regulated.
 * @return {string} Name in the desired format.
 */
function regulatePictureName(entry) {
  if (hasVideoPrefix(entry) || hasImagePrefix(entry)) {
    const match = entry.name.match(/(\w{3}_\d{8}_\d{6})(?:_(\d+))?(\..+)?$/);
    if (match) {
      const idx = match[2] ? ' (' + match[2] + ')' : '';
      const ext = match[3] ? match[3].replace(/\.webm$/, '.mkv') : '';
      return match[1] + idx + ext;
    }
  } else {
    // Early pictures are in legacy file name format (crrev.com/c/310064).
    const match = entry.name.match(/(\d+).(?:\d+)/);
    if (match) {
      return (new Filenamer(parseInt(match[1], 10))).newImageName();
    }
  }
  return entry.name;
}

/**
 * Migrates all picture-files except thumbnails from internal storage to
 * external storage. For thumbnails, we just remove them.
 * @return {!Promise} Promise for the operation.
 */
async function migratePictures() {
  const internalEntries = await internalDir.getFiles();
  for (const entry of internalEntries) {
    if (entry.name.startsWith(THUMBNAIL_PREFIX)) {
      await entry.remove();
      continue;
    }
    const name = regulatePictureName(entry);
    assert(externalDir !== null);
    await entry.moveTo(externalDir, name);
  }
}

/**
 * Initializes file systems. This function should be called only once in the
 * beginning of the app.
 * @return {!Promise}
 */
export async function initialize() {
  internalDir = await initInternalDir();
  assert(internalDir !== null);

  internalTempDir = await initInternalTempDir();
  assert(internalTempDir !== null);

  externalDir = await initExternalDir();
  assert(externalDir !== null);
}

/**
 * Checks and performs migration if it's needed.
 * @param {function(): !Promise} promptMigrate Callback to instantiate a promise
 *     that prompts users to migrate pictures if no acknowledgement yet.
 * @return {!Promise<boolean>} Return a promise that will be resolved to a
 *     boolean indicates if the user ackes the migration dialog once the
 *     migration is skipped or completed.
 */
export async function checkMigration(promptMigrate) {
  const isDoneMigration =
      (await browserProxy.localStorageGet({doneMigration: 0}))['doneMigration'];
  if (isDoneMigration) {
    return false;
  }

  const doneMigrate = () => browserProxy.localStorageSet({doneMigration: 1});
  const ackMigrate = () =>
      browserProxy.localStorageSet({ackMigratePictures: 1});

  const internalEntries = await internalDir.getFiles();
  const migrationNeeded = internalEntries.length > 0;
  if (!migrationNeeded) {
    // If there is already no picture in the internal file system, it implies
    // done migration and then doesn't need acknowledge-prompt.
    await ackMigrate();
    await doneMigrate();
    return false;
  }

  const isAckedMigration = (await browserProxy.localStorageGet(
      {ackMigratePictures: 0}))['ackMigratePictures'];
  if (!isAckedMigration) {
    await promptMigrate();
    await ackMigrate();
  }
  await migratePictures();
  await doneMigrate();

  return !isAckedMigration;
}

/**
 * Saves photo blob or metadata blob into predefined default location.
 * @param {!Blob} blob Data of the photo to be saved.
 * @param {string} name Filename of the photo to be saved.
 * @return {!Promise<?AbstractFileEntry>} Promise for the result.
 */
export async function saveBlob(blob, name) {
  assert(externalDir !== null);

  const file = await externalDir.createFile(name);
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
  assert(externalDir !== null);
  const name = new Filenamer().newVideoName();
  const file = await externalDir.createFile(name);
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
  const entries = await externalDir.getFiles();
  return entries.filter((entry) => {
    if (!hasVideoPrefix(entry) && !hasImagePrefix(entry)) {
      return false;
    }
    return entry.name.match(/_(\d{8})_(\d{6})(?: \((\d+)\))?/);
  });
}

/**
 * Returns an URL for a picture given by the file |entry|. Optionally, if
 * |limit| is specified, the file would be truncated if it's larger than it.
 * @param {!AbstractFileEntry} entry The file entry of the picture.
 * @param {{limit: (number|undefined)}=} options
 * @return {!Promise<string>} Promise for the result.
 */
export async function pictureURL(entry, {limit = Infinity} = {}) {
  let file = await entry.file();
  if (file.size > limit) {
    file = file.slice(0, limit);
  }
  return URL.createObjectURL(file);
}
