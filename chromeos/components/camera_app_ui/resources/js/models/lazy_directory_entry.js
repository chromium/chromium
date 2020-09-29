// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  AbstractDirectoryEntry,  // eslint-disable-line no-unused-vars
} from './file_system_entry.js';

/**
 * Gets directory entry by given |name| under |parentDir| directory. If the
 * directory does not exist, returns a lazy directory which will only be created
 * once there is any file written in it.
 * @param {!AbstractDirectoryEntry} parentDir Parent directory.
 * @param {string} name Name of the target directory.
 * @return {!Promise<!AbstractDirectoryEntry>}
 */
export async function getMaybeLazyDirectory(parentDir, name) {
  const targetDir =
      await parentDir.getDirectory({name, createIfNotExist: false});
  return targetDir !== null ? targetDir :
                              new LazyDirectoryEntry(parentDir, name);
}

/**
 * A directory entry which will only create itself if there is any
 * file/directory created under it.
 * @implements {AbstractDirectoryEntry}
 */
class LazyDirectoryEntry {
  /**
   * @param {!AbstractDirectoryEntry} parentDirectory
   * @param {string} name
   */
  constructor(parentDirectory, name) {
    /**
     * @type {!AbstractDirectoryEntry}
     * @private
     */
    this.parent_ = parentDirectory;

    /**
     * @type {string}
     * @private
     */
    this.name_ = name;

    /**
     * @type {?AbstractDirectoryEntry}
     * @private
     */
    this.directory_ = null;

    /**
     * @type {?Promise<!AbstractDirectoryEntry>}
     * @private
     */
    this.creatingDirectory_ = null;
  }

  /**
   * The name of the directory that will lazily created.
   * @return {string}
   */
  get name() {
    return this.name_;
  }

  /**
   * @override
   */
  async getFiles() {
    if (this.directory_ === null) {
      return [];
    }
    return this.directory_.getFiles();
  }

  /**
   * @override
   */
  async getDirectories() {
    if (this.directory_ === null) {
      return [];
    }
    return this.directory_.getDirectories();
  }

  /**
   * @override
   */
  async getFile(name) {
    if (this.directory_ === null) {
      return null;
    }
    return this.directory_.getFile(name);
  }

  /**
   * @override
   */
  async createFile(name) {
    const dir = await this.getRealDirectory_();
    return dir.createFile(name);
  }

  /**
   * @override
   */
  async getDirectory({name, createIfNotExist}) {
    if (this.directory_ === null && !createIfNotExist) {
      return null;
    }
    const dir = await this.getRealDirectory_();
    return dir.getDirectory({name, createIfNotExist});
  }

  /**
   * Gets the directory which this entry points to. Create it if it does not
   * exist.
   * @return {!Promise<!AbstractDirectoryEntry>}
   * @private
   */
  async getRealDirectory_() {
    if (this.creatingDirectory_ === null) {
      this.creatingDirectory_ =
          (async () => /** @type {!AbstractDirectoryEntry} */ (
               await this.parent_.getDirectory(
                   {name: this.name_, createIfNotExist: true})))();
    }
    this.directory_ =
        /** @type {!AbstractDirectoryEntry} */ (await this.creatingDirectory_);
    return this.directory_;
  }
}
