// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  AbstractDirectoryEntry,   // eslint-disable-line no-unused-vars
  AbstractFileEntry,        // eslint-disable-line no-unused-vars
  AbstractFileSystemEntry,  // eslint-disable-line no-unused-vars
} from '../models/file_system_entry.js';
// eslint-disable-next-line no-unused-vars
import {UntrustedOrigin} from '../type.js';

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
  async getCameraDirectory() {}

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
   * @return {!Promise}
   * @abstract
   */
  async localStorageClear() {}

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
   * @return {string} Returns 'ltr'/'rtl' for left-to-right/right-to-left system
   *     UI language.
   * @abstract
   */
  getTextDirection() {}

  /**
   * @return {boolean}
   * @abstract
   */
  shouldAddFakeHistory() {}

  /**
   * @abstract
   */
  openFeedback() {}

  /**
   * @param {function(): !Promise} exploitUsage
   * @param {function(): !Promise} releaseUsage
   * @abstract
   */
  async initCameraUsageMonitor(exploitUsage, releaseUsage) {}

  /**
   * @param {function(): void} listener
   * @abstract
   */
  setupUnloadListener(listener) {}

  /**
   * @param {function(): !Promise} callback
   * @return {!Promise}
   * @abstract
   */
  async setLaunchingFromWindowCreationStartTime(callback) {}

  /**
   * @return {!UntrustedOrigin}
   * @abstract
   */
  getUntrustedOrigin() {}

  /**
   * @param {boolean} enabled
   * @abstract
   */
  setBeforeUnloadListenerEnabled(enabled) {}
}
