// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '../chrome_util.js';
import {NotImplementedError} from '../error.js';

import {AsyncWriter} from './async_writer.js';
import {
  AbstractDirectoryEntry,   // eslint-disable-line no-unused-vars
  AbstractFileEntry,        // eslint-disable-line no-unused-vars
  AbstractFileSystemEntry,  // eslint-disable-line no-unused-vars
} from './file_system_entry.js';

/**
 * The file system entry implementation for SWA.
 * @implements {AbstractFileSystemEntry}
 */
export class NativeFileSystemEntry {
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
   * @override
   */
  get name() {
    return this.handle_.name;
  }
}

/**
 * The file entry implementation for SWA.
 * @implements {AbstractFileEntry}
 */
export class NativeFileEntry extends NativeFileSystemEntry {
  /**
   * @param {!FileSystemFileHandle} handle
   */
  constructor(handle) {
    super(handle);

    /**
     * @type {!FileSystemFileHandle}
     * @private
     */
    this.handle_ = handle;
  }

  /**
   * @override
   */
  async file() {
    return this.handle_.getFile();
  }

  /**
   * @override
   */
  async write(blob) {
    const writer = await this.handle_.createWritable();
    await writer.write(blob);
    await writer.close();
  }

  /**
   * @override
   */
  async getWriter() {
    const writer = await this.handle_.createWritable();
    // TODO(crbug.com/980846): We should write files in-place so that even the
    // app is accidentally closed or hit any unexpected exceptions, the captured
    // video will not be dropped entirely.
    const doWrite = (blob) => writer.write(blob);
    return new AsyncWriter(doWrite, {
      onClosed: () => writer.close(),
    });
  }

  /**
   * @override
   */
  async moveTo(dir, name) {
    throw new NotImplementedError();
  }

  /**
   * @override
   */
  async remove() {
    throw new NotImplementedError();
  }

  /**
   * @override
   */
  async getLastModificationTime() {
    const file = await this.file();
    return file.lastModified;
  }

  /**
   * @override
   */
  getRawEntry() {
    throw new NotImplementedError();
  }
}

/**
 * The directory entry implementation for SWA.
 * @implements {AbstractDirectoryEntry}
 */
export class NativeDirectoryEntry extends NativeFileSystemEntry {
  /**
   * @param {!FileSystemDirectoryHandle} handle
   */
  constructor(handle) {
    super(handle);

    /**
     * @type {!FileSystemDirectoryHandle}
     * @private
     */
    this.handle_ = handle;
  }

  /**
   * @override
   */
  async getFiles() {
    return this.getHandles_({isDirectory: false});
  }

  /**
   * @override
   */
  async getDirectories() {
    return this.getHandles_({isDirectory: true});
  }

  /**
   * @override
   */
  async getFile(name) {
    const handle = await this.handle_.getFileHandle(name, {create: false});
    return new NativeFileEntry(handle);
  }

  /**
   * @override
   */
  async createFile(name) {
    const handle = await this.handle_.getFileHandle(name, {create: true});
    return new NativeFileEntry(handle);
  }

  /**
   * @override
   */
  async getDirectory({name, createIfNotExist}) {
    const handle =
        await this.handle_.getDirectoryHandle(name, {create: createIfNotExist});
    assert(handle !== null);
    return new NativeDirectoryEntry(
        /** @type {!FileSystemDirectoryHandle} */ (handle));
  }

  /**
   * Gets the file handles in this directory if |isDirectory| is set to false.
   * If |isDirectory| is true, gets the directory entries instead.
   * @param {{isDirectory: boolean}} params
   * @return {!Promise<!Array<!AbstractFileSystemEntry>>}
   */
  async getHandles_({isDirectory}) {
    const results = [];
    for await (const handle of this.handle_.values()) {
      if (isDirectory && handle.kind === 'directory') {
        results.push(new NativeDirectoryEntry(handle));
      } else if (!isDirectory && handle.kind === 'file') {
        results.push(new NativeFileEntry(handle));
      }
    }
    return results;
  }
}
