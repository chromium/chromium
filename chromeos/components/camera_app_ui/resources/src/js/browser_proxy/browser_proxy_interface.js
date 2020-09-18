// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// eslint-disable-next-line no-unused-vars
import {BackgroundOps} from '../background_ops.js';
import {
  AbstractDirectoryEntry,   // eslint-disable-line no-unused-vars
  AbstractFileEntry,        // eslint-disable-line no-unused-vars
  AbstractFileSystemEntry,  // eslint-disable-line no-unused-vars
} from '../models/file_system_entry.js';

/**
 * The abstract interface for the CCA's interaction with the browser.
 * @interface
 */
export class BrowserProxy {
  /**
   * @return {!Promise<boolean>}
   * @abstract
   */
  async requestEnumerateDevicesPermission() {}

  /**
   * @return {!Promise<?AbstractDirectoryEntry>}
   * @abstract
   */
  async getExternalDir() {}

  /**
   * @param {(string|!Array<string>|!Object)} keys
   * @return {!Promise<!Object>}
   * @abstract
   */
  async localStorageGet(keys) {}

  /**
   * @param {!Object<string>} items
   * @return {!Promise}
   * @abstract
   */
  async localStorageSet(items) {}

  /**
   * @param {(string|!Array<string>)} items
   * @return {!Promise}
   * @abstract
   */
  async localStorageRemove(items) {}

  /**
   * @return {!Promise<string>}
   * @abstract
   */
  async getBoard() {}

  /**
   * @param {string} name
   * @param {...(string|number)} substitutions
   * @return {string}
   * @abstract
   */
  getI18nMessage(name, ...substitutions) {}

  /**
   * @param {function(boolean)} callback
   * @abstract
   */
  addOnLockListener(callback) {}

  /**
   * @return {!Promise<boolean>}
   * @abstract
   */
  async isMetricsAndCrashReportingEnabled() {}

  /**
   * @param {!AbstractFileEntry} file
   * @return {!Promise}
   * @abstract
   */
  async openGallery(file) {}

  /**
   * @param {string} type
   * @abstract
   */
  openInspector(type) {}

  /**
   * @return {string}
   * @abstract
   */
  getAppVersion() {}

  /**
   * @param {function(*, !MessageSender, function(string)): (boolean|undefined)}
   *     listener
   * @abstract
   */
  addOnMessageExternalListener(listener) {}

  /**
   * @param {function(!Port)} listener
   * @abstract
   */
  addOnConnectExternalListener(listener) {}

  /**
   * @abstract
   */
  addDummyHistoryIfNotAvailable() {}

  /**
   * @return {boolean}
   * @abstract
   */
  isMp4RecordingEnabled() {}

  /**
   * @return {!BackgroundOps}
   * @abstract
   */
  getBackgroundOps() {}

  /**
   * @return {boolean}
   * @abstract
   */
  isFullscreenOrMaximized() {}

  /**
   * @return {!Promise}
   * @abstract
   */
  async fitWindow() {}

  /**
   * @abstract
   */
  showWindow() {}

  /**
   * @abstract
   */
  hideWindow() {}

  /**
   * @return {boolean}
   * @abstract
   */
  isMinimized() {}

  /**
   * @param {function(): void} listener
   * @abstract
   */
  addOnMinimizedListener(listener) {}

  /**
   * @abstract
   */
  openFeedback() {}
}
