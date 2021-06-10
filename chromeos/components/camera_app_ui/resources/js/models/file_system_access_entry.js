// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AsyncJobQueue} from '../async_job_queue.js';
import {assert} from '../chrome_util.js';

import {AsyncWriter} from './async_writer.js';

/**
 * The file system entry implementation for SWA.
 */
export class FileSystemAccessEntry {
  /**
   * @param {!FileSystemHandle} handle
   */
  constructor(handle) {
    /**
     * @type {!FileSystemHandle}
     * @private
     */
    this.handle_ = handle;
  }

  /**
   * @return {string}
   */
  get name() {
    return this.handle_.name;
  }
}

/**
 * The file entry implementation for SWA.
 */
export class FileAccessEntry extends FileSystemAccessEntry {
  /**
   * @param {!FileSystemFileHandle} handle
   * @param {?DirectoryAccessEntryImpl} parent
   */
  constructor(handle, parent = null) {
    super(handle);

    /**
     * @type {!FileSystemFileHandle}
     * @private
     */
    this.handle_ = handle;

    /**
     * @type {?DirectoryAccessEntryImpl}
     * @private
     */
    this.parent_ = parent;
  }

  /**
   * Returns the File object which the entry points to.
   * @return {!Promise<!File>}
   */
  async file() {
    return this.handle_.getFile();
  }

  /**
   * Writes |blob| data into the file.
   * @param {!Blob} blob
   * @return {!Promise} The returned promise is resolved once the write
   *     operation is completed.
   */
  async write(blob) {
    const writer = await this.handle_.createWritable();
    await writer.write(blob);
    await writer.close();
  }

  /**
   * Gets a writer to write data into the file.
   * @return {!Promise<!AsyncWriter>}
   */
  async getWriter() {
    const writer = await this.handle_.createWritable();
    // TODO(crbug.com/980846): We should write files in-place so that even the
    // app is accidentally closed or hit any unexpected exceptions, the captured
    // video will not be dropped entirely.
    return new AsyncWriter({
      write: (blob) => writer.write(blob),
      seek: (offset) => writer.seek(offset),
      close: () => writer.close(),
    });
  }

  /**
   * Gets the timestamp of the last modification time of the file.
   * @return {!Promise<number>} The number of milliseconds since the Unix epoch
   *     in UTC.
   */
  async getLastModificationTime() {
    const file = await this.file();
    return file.lastModified;
  }

  /**
   * Deletes the file.
   * @return {!Promise}
   * @throws {!Error} Thrown when trying to delete file with no parent
   *     directory.
   */
  async delete() {
    if (this.parent_ === null) {
      throw new Error('Failed to delete file due to no parent directory');
    }
    return this.parent_.removeEntry(this.name);
  }
}

/**
 * Guards from name collision when creating files.
 * @type {!AsyncJobQueue}
 */
const createFileJobs = new AsyncJobQueue();

/**
 * The abstract interface for the directory entry.
 * @interface
 */
export class DirectoryAccessEntry {
  /* eslint-disable getter-return */

  /**
   * Gets the name of the directory.
   * @return {string}
   * @abstract
   */
  get name() {}

  /* eslint-enable getter-return */

  /**
   * Gets files in this directory.
   * @return {!Promise<!Array<!FileAccessEntry>>}
   * @abstract
   */
  async getFiles() {}

  /**
   * Gets directories in this directory.
   * @return {!Promise<!Array<!DirectoryAccessEntry>>}
   * @abstract
   */
  async getDirectories() {}

  /**
   * Gets the file given by its |name|.
   * @param {string} name The name of the file.
   * @return {!Promise<?FileAccessEntry>} The entry of the found file.
   * @abstract
   */
  async getFile(name) {}

  /**
   * Checks if file or directory with the target name exists.
   * @param {string} name
   * @return {!Promise<boolean>}
   * @abstract
   */
  async isExist(name) {}

  /**
   * Create the file given by its |name|. If there is already a file with same
   * name, it will try to use a name with index as suffix.
   * e.g. IMG.png => IMG (1).png
   * @param {string} name The name of the file.
   * @return {!Promise<!FileAccessEntry>} The entry of the created file.
   * @abstract
   */
  async createFile(name) {}

  /**
   * Gets the directory given by its |name|. If the directory is not found,
   * create one if |createIfNotExist| is true.
   * TODO(crbug.com/1127587): Split this method to getDirectory() and
   * createDirectory().
   * @param {{name: string, createIfNotExist: boolean}} params
   * @return {!Promise<?DirectoryAccessEntry>} The entry of the found/created
   *     directory.
   */
  async getDirectory({name, createIfNotExist}) {}

  /**
   * Removes file by given |name| from the directory.
   * @param {string} name The name of the file.
   * @return {!Promise}
   */
  async removeEntry(name) {}
}

/**
 * The directory entry implementation for SWA.
 * @implements {DirectoryAccessEntry}
 */
export class DirectoryAccessEntryImpl extends FileSystemAccessEntry {
  /**
   * @param {!FileSystemDirectoryHandle} handle
   * @param {?DirectoryAccessEntryImpl} parent
   */
  constructor(handle, parent = null) {
    super(handle);

    /**
     * @type {!FileSystemDirectoryHandle}
     * @private
     */
    this.handle_ = handle;

    /**
     * @type {?DirectoryAccessEntryImpl}
     * @private
     */
    this.parent_ = parent;
  }

  /**
   * @override
   */
  get name() {
    return this.name;
  }

  /**
   * @override
   */
  async getFiles() {
    return /** @type {!Array<!FileAccessEntry>} */ (
        await this.getHandles_({isDirectory: false}));
  }

  /**
   * @override
   */
  async getDirectories() {
    return /** @type {!Array<!DirectoryAccessEntry>} */ (
        await this.getHandles_({isDirectory: true}));
  }

  /**
   * @override
   */
  async getFile(name) {
    const handle = await this.handle_.getFileHandle(name, {create: false});
    return new FileAccessEntry(handle, this);
  }

  /**
   * @override
   */
  async isExist(name) {
    try {
      await this.getFile(name);
      return true;
    } catch (e) {
      if (e.name === 'NotFoundError') {
        return false;
      }
      if (e.name === 'TypeMismatchError' || e instanceof TypeError) {
        // Directory with same name exists.
        return true;
      }
      throw e;
    }
  }

  /**
   * @override
   */
  async createFile(name) {
    return createFileJobs.push(async () => {
      let uniqueName = name;
      for (let i = 0; await this.isExist(uniqueName);) {
        uniqueName = name.replace(/^(.*?)(?=\.)/, `$& (${++i})`);
      }
      const handle =
          await this.handle_.getFileHandle(uniqueName, {create: true});
      return new FileAccessEntry(handle, this);
    });
  }

  /**
   * @override
   */
  async getDirectory({name, createIfNotExist}) {
    try {
      const handle = await this.handle_.getDirectoryHandle(
          name, {create: createIfNotExist});
      assert(handle !== null);
      return new DirectoryAccessEntryImpl(
          /** @type {!FileSystemDirectoryHandle} */ (handle), this);
    } catch (error) {
      if (!createIfNotExist && error.name === 'NotFoundError') {
        return null;
      }
      throw error;
    }
  }

  /**
   * @override
   */
  async removeEntry(name) {
    return this.handle_.removeEntry(name);
  }

  /**
   * Gets the file handles in this directory if |isDirectory| is set to false.
   * If |isDirectory| is true, gets the directory entries instead.
   * @param {{isDirectory: boolean}} params
   * @return {!Promise<!Array<!FileSystemAccessEntry>>}
   */
  async getHandles_({isDirectory}) {
    const results = [];
    for await (const handle of this.handle_.values()) {
      if (isDirectory && handle.kind === 'directory') {
        results.push(new DirectoryAccessEntryImpl(handle, this));
      } else if (!isDirectory && handle.kind === 'file') {
        results.push(new FileAccessEntry(handle, this));
      }
    }
    return results;
  }
}
