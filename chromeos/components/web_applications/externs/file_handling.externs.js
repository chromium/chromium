// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview @externs
 * Externs for interfaces in //third_party/blink/renderer/modules/launch/*
 * This file can be removed when upstreamed to the closure compiler.
 */

/** @interface */
class FileSystemWriter {
  /**
   * @param {number} position
   * @param {BufferSource|Blob|string} data
   * @return {!Promise<undefined>}
   */
  async write(position, data) {}

  /**
   * @param {number} size
   * @return {!Promise<undefined>}
   */
  async truncate(size) {}

  /**
   * @return {!Promise<undefined>}
   */
  async close() {}
}

/**
 * @typedef {{
 *   type: string,
 *   size: (number|undefined),
 *   position: (number|undefined),
 *   data: (BufferSource|Blob|string|undefined)
 * }}
 */
let WriteParams;

/** @interface */
class FileSystemWritableFileStream {
  /**
   * @param {BufferSource|Blob|string|WriteParams} data
   * @return {!Promise<undefined>}
   */
  async write(data) {}

  /**
   * @param {number} size
   * @return {!Promise<undefined>}
   */
  async truncate(size) {}

  /**
   * @return {!Promise<undefined>}
   */
  async close() {}

  /**
   * @param {number} offset
   * @return {!Promise<undefined>}
   */
  async seek(offset) {}
}

/** @typedef {{mode: string}} */
let FileSystemHandlePermissionDescriptor;

/** @interface */
class FileSystemHandle {
  constructor() {
    /** @type {string} */
    this.kind;

    /** @type {string} */
    this.name;
  }

  /**
   * @param {FileSystemHandle} other
   * @return {!Promise<boolean>}
   */
  isSameEntry(other) {}

  /**
   * @param {FileSystemHandlePermissionDescriptor} descriptor
   * @return {!Promise<PermissionState>}
   */
  queryPermission(descriptor) {}

  /**
   * @param {FileSystemHandlePermissionDescriptor} descriptor
   * @return {!Promise<PermissionState>}
   */
  requestPermission(descriptor) {}
}

/** @typedef {{keepExistingData: boolean}} */
let FileSystemCreateWritableOptions;

/** @interface */
class FileSystemFileHandle extends FileSystemHandle {
  /**
   * @param {FileSystemCreateWritableOptions=} options
   * @return {!Promise<!FileSystemWritableFileStream>}
   */
  createWritable(options) {}

  /** @return {!Promise<!File>} */
  getFile() {}
}

/** @typedef {{create: boolean}} */
let FileSystemGetFileOptions;

/** @typedef {{create: boolean}} */
let FileSystemGetDirectoryOptions;

/** @typedef {{recursive: boolean}} */
let FileSystemRemoveOptions;

/** @typedef {{type: string}} */
let GetSystemDirectoryOptions;

/** @interface */
class FileSystemDirectoryHandle extends FileSystemHandle {
  /**
   * @param {string} name
   * @param {FileSystemGetFileOptions=} options
   * @return {!Promise<!FileSystemFileHandle>}
   */
  getFileHandle(name, options) {}

  /**
   * @param {string} name
   * @param {FileSystemGetDirectoryOptions=} options
   * @return {Promise<!FileSystemDirectoryHandle>}
   */
  getDirectoryHandle(name, options) {}

  /** @return {!AsyncIterable<!Array<string|!FileSystemHandle>>} */
  entries() {}
  /** @return {!AsyncIterable<string>} */
  keys() {}
  /** @return {!AsyncIterable<!FileSystemHandle>} */
  values() {}

  /**
   * @param {string} name
   * @param {FileSystemRemoveOptions=} options
   * @return {Promise<undefined>}
   */
  removeEntry(name, options) {}
}

/** @interface */
class LaunchParams {
  constructor() {
    /** @type {Array<FileSystemHandle>} */
    this.files;

    /** @type {Request} */
    this.request;
  }
}

/** @typedef {function(LaunchParams)} */
let LaunchConsumer;

/** @interface */
class LaunchQueue {
  /** @param {LaunchConsumer} consumer */
  setConsumer(consumer) {}
}

/**
 * https://wicg.github.io/native-file-system/#dictdef-filepickeraccepttype
 * @typedef {{
 *    description: string,
 *    accept: !Array<!Object<string, Array<string>>>,
 * }}
 */
let FilePickerAcceptType;

/**
 * https://wicg.github.io/native-file-system/#dictdef-filepickeroptions
 * https://wicg.github.io/native-file-system/#dictdef-directorypickeroptions
 * https://wicg.github.io/native-file-system/#dictdef-openfilepickeroptions
 * https://wicg.github.io/native-file-system/#dictdef-savefilepickeroptions
 * Note: `multiple` is only used for openfilepicker.
 * `types` is required if excludeAcceptAllOption is true.
 * @typedef {{
 *    multiple: (boolean|undefined),
 *    types: (!Array<!FilePickerAcceptType>|undefined),
 *    excludeAcceptAllOption: (boolean|undefined)
 * }}
 */
let FilePickerOptions;

/**
 * https://wicg.github.io/native-file-system/#native-filesystem
 * @param {(!FilePickerOptions|undefined)} options
 * @return {!Promise<(!Array<!FileSystemFileHandle>)>}
 */
window.showOpenFilePicker;

/**
 * https://wicg.github.io/native-file-system/#native-filesystem
 * @param {(!FilePickerOptions|undefined)} options
 * @return {!Promise<(!FileSystemFileHandle)>}
 */
window.showSaveFilePicker;

/**
 * https://wicg.github.io/native-file-system/#native-filesystem
 * @param {(!FilePickerOptions|undefined)} options
 * @return {!Promise<(!FileSystemDirectoryHandle)>}
 */
window.showDirectoryPicker;

/** @type {LaunchQueue} */
window.launchQueue;
