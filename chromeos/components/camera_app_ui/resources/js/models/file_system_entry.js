// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// eslint-disable-next-line no-unused-vars
import {AsyncWriter} from './async_writer.js';

/**
 * The abstract interface for the file system entry that is applicable for
 * platform app and SWA.
 * @interface
 */
export class AbstractFileSystemEntry {
  /* eslint-disable getter-return */

  /**
   * The name of the file/directory.
   * @return {string}
   * @abstract
   */
  get name() {}

  /* eslint-enable getter-return */
}

/**
 * The abstract interface for the file entry that is applicable for platform app
 * and SWA.
 * @interface
 */
export class AbstractFileEntry extends AbstractFileSystemEntry {
  /**
   * Returns the File object which the entry points to.
   * @return {!Promise<!File>}
   * @abstract
   */
  async file() {}

  /**
   * Writes |blob| data into the file.
   * @param {!Blob} blob
   * @return {!Promise} The returned promise is resolved once the write
   *     operation is completed.
   * @abstract
   */
  async write(blob) {}

  /**
   * Gets a writer to write data into the file.
   * @return {!Promise<!AsyncWriter>}
   */
  async getWriter() {}

  /**
   * Moves the file to |dir| with the new |name| and return the new entry. The
   * original entry should not be used after calling this method.
   * @param {!AbstractDirectoryEntry} dir
   * @param {string} name
   * @return {!Promise<!AbstractFileEntry>} The new file entry.
   * @abstract
   */
  async moveTo(dir, name) {}

  /**
   * Removes the file. The entry should not be used after calling this method.
   * @return {!Promise}
   * @abstract
   */
  async remove() {}

  /**
   * Gets the timestamp of the last modification time of the file.
   * @return {!Promise<number>} The number of milliseconds since the Unix epoch
   *     in UTC.
   * @abstract
   */
  async getLastModificationTime() {}

  /**
   * Gets the raw FileEntry object which is wrapped by the entry.
   * @return {!FileEntry}
   * @abstract
   */
  getRawEntry() {}
}

/**
 * The abstract interface for the directory entry that is applicable for
 * platform app and SWA.
 * @interface
 */
export class AbstractDirectoryEntry extends AbstractFileSystemEntry {
  /**
   * Gets files in this directory.
   * @return {!Promise<!Array<!AbstractFileEntry>>}
   * @abstract
   */
  async getFiles() {}

  /**
   * Gets directories in this directory.
   * @return {!Promise<!Array<!AbstractDirectoryEntry>>}
   * @abstract
   */
  async getDirectories() {}

  /**
   * Gets the file given by its |name|.
   * @param {string} name The name of the file.
   * @return {!Promise<?AbstractFileEntry>} The entry of the found file.
   * @abstract
   */
  async getFile(name) {}

  /**
   * Create the file given by its |name|. If there is already a file with same
   * name, it will try to use a name with index as suffix.
   * e.g. IMG.png => IMG (1).png
   * @param {string} name The name of the file.
   * @return {!Promise<!AbstractFileEntry>} The entry of the created file.
   * @abstract
   */
  async createFile(name) {}

  /**
   * Gets the directory given by its |name|. If the directory is not found,
   * create one if |createIfNotExist| is true.
   * @param {{name: string, createIfNotExist: boolean}} params
   * @return {!Promise<?AbstractDirectoryEntry>} The entry of the found/created
   *     directory.
   */
  async getDirectory({name, createIfNotExist}) {}
}
