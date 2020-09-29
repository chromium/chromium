// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {promisify, promisifyWithError} from '../chrome_util.js';
import {AsyncWriter} from './async_writer.js';
import {
  AbstractDirectoryEntry,   // eslint-disable-line no-unused-vars
  AbstractFileEntry,        // eslint-disable-line no-unused-vars
  AbstractFileSystemEntry,  // eslint-disable-line no-unused-vars
} from './file_system_entry.js';
import {incrementFileName} from './file_util.js';

/**
 * The file system entry implementation for platform app.
 * @implements {AbstractFileSystemEntry}
 */
export class ChromeFileSystemEntry {
  /**
   * @param {!Entry} entry
   */
  constructor(entry) {
    /**
     * @type {!Entry}
     * @private
     */
    this.entry_ = entry;
  }

  /**
   * @override
   */
  get name() {
    return this.entry_.name;
  }
}

/**
 * The file entry implementation for platform app.
 * @implements {AbstractFileEntry}
 */
export class ChromeFileEntry extends ChromeFileSystemEntry {
  /**
   * @param {!FileEntry} entry
   */
  constructor(entry) {
    super(entry);

    /**
     * @type {!FileEntry}
     * @private
     */
    this.entry_;

    /**
     * @type {{
     *   file: function(): !Promise,
     *   createWriter: function(): !Promise,
     *   remove: function(): !Promise,
     *   getMetadata: function(): !Promise,
     * }}
     * @private
     */
    this.entry_ops_ = {
      file: promisify(entry.file.bind(entry)),
      createWriter: promisifyWithError(entry.createWriter.bind(entry)),
      remove: promisifyWithError(entry.remove.bind(entry)),
      getMetadata: promisifyWithError(entry.getMetadata.bind(entry)),
    };
  }

  /**
   * @override
   */
  async file() {
    return this.entry_ops_.file();
  }

  /**
   * @override
   */
  async write(blob) {
    const writer = await this.getWriter();
    return writer.write(blob);
  }

  /**
   * @override
   */
  async getWriter() {
    const fileWriter = await this.entry_ops_.createWriter();
    const doWrite = (blob) => new Promise((resolve, reject) => {
      fileWriter.onwriteend = resolve;
      fileWriter.onerror = reject;
      fileWriter.write(blob);
    });
    return new AsyncWriter(doWrite);
  }

  /**
   * @override
   */
  async moveTo(dir, name) {
    const targetFile = await dir.createFile(name);
    const blob = await this.file();
    await targetFile.write(blob);
    await this.entry_ops_.remove();
    return targetFile;
  }

  /**
   * @override
   */
  async remove() {
    return this.entry_ops_.remove();
  }

  /**
   * @override
   */
  async getLastModificationTime() {
    const metadata = await this.entry_ops_.getMetadata();
    return metadata.modificationTime.getTime();
  }

  /**
   * @override
   * @return {!FileEntry}
   */
  getRawEntry() {
    return this.entry_;
  }
}

/**
 * The directory entry implementation for platform app.
 * @implements {AbstractDirectoryEntry}
 */
export class ChromeDirectoryEntry extends ChromeFileSystemEntry {
  /**
   * @param {!DirectoryEntry} entry
   */
  constructor(entry) {
    super(entry);

    /**
     * @type {!DirectoryEntry}
     * @private
     */
    this.entry_;

    /**
     * @type {{
     *   getFile: function(string, !Object): !Promise,
     *   getDirectory: function(string, !Object): !Promise,
     * }}
     * @private
     */
    this.entry_ops_ = {
      getFile: promisifyWithError(entry.getFile.bind(entry)),
      getDirectory: promisifyWithError(entry.getDirectory.bind(entry)),
    };
  }

  /**
   * @override
   */
  async getFiles() {
    return this.getEntries_({isDirectory: false});
  }

  /**
   * @override
   */
  async getDirectories() {
    return this.getEntries_({isDirectory: true});
  }

  /**
   * @override
   */
  async getFile(name) {
    try {
      const file = await this.entry_ops_.getFile(name, {create: false});
      return new ChromeFileEntry(file);
    } catch (error) {
      if (error.name === 'NotFoundError') {
        return null;
      }
      throw error;
    }
  }

  /**
   * @override
   */
  async createFile(name) {
    try {
      const file =
          await this.entry_ops_.getFile(name, {create: true, exclusive: true});
      return new ChromeFileEntry(file);
    } catch (error) {
      if (error.name === 'InvalidModificationError') {
        // Avoid name conflicts for creating files.
        return this.createFile(incrementFileName(name));
      }
      throw error;
    }
  }

  /**
   * @override
   */
  async getDirectory({name, createIfNotExist}) {
    const options =
        createIfNotExist ? {create: true, exclusive: false} : {create: false};
    try {
      const file = await this.entry_ops_.getDirectory(name, options);
      return new ChromeDirectoryEntry(file);
    } catch (error) {
      if (!createIfNotExist && error.name === 'NotFoundError') {
        return null;
      }
      throw error;
    }
  }

  /**
   * Gets the file entries in this directory if |isDirectory| is set to false.
   * If |isDirectory| is true, gets the directory entries instead.
   * @param {{isDirectory: boolean}} params
   * @return {!Promise<!Array<!AbstractFileSystemEntry>>}
   */
  async getEntries_({isDirectory}) {
    const dirReader = this.entry_.createReader();
    const entries = [];

    /* eslint no-constant-condition: ["error", { "checkLoops": false }] */
    while (true) {
      const inEntries =
          await promisifyWithError(dirReader.readEntries.bind(dirReader))();
      if (inEntries.length === 0) {
        break;
      }

      for (const entry of inEntries) {
        if (entry.isDirectory !== isDirectory) {
          continue;
        }
        entries.push(
            isDirectory ?
                new ChromeDirectoryEntry(
                    /** @type {!DirectoryEntry} */ (entry)) :
                new ChromeFileEntry(/** @type {!FileEntry} */ (entry)));
      }
    }
    return entries;
  }
}
