// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * strings.m.js is generated when we enable it via UseStringsJs() in webUI
 * controller. When loading it, it will populate data such as localized strings
 * into |window.loadTimeData|.
 * @suppress {moduleLoad}
 */
import '/strings.m.js';

import {assert} from '../chrome_util.js';
import * as idb from '../models/idb.js';
import {getMaybeLazyDirectory} from '../models/lazy_directory_entry.js';
import {NativeDirectoryEntry} from '../models/native_file_system_entry.js';
import {ChromeHelper} from '../mojo/chrome_helper.js';
import {UntrustedOrigin} from '../type.js';
import {WaitableEvent} from '../waitable_event.js';

// eslint-disable-next-line no-unused-vars
import {BrowserProxy} from './browser_proxy_interface.js';

/**
 * The 'beforeunload' listener which will show confirm dialog when trying to
 * close window.
 * @param {!Event} event The 'beforeunload' event.
 */
function beforeUnloadListener(event) {
  event.preventDefault();
  event.returnValue = '';
}

/**
 * The WebUI implementation of the CCA's interaction with the browser.
 * @implements {BrowserProxy}
 */
class WebUIBrowserProxy {
  /** @override */
  async requestEnumerateDevicesPermission() {
    // No operation here since the permission is automatically granted for
    // the chrome:// scheme.
    return true;
  }

  /** @override */
  async getCameraDirectory() {
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

  /** @override */
  async localStorageGet(keys) {
    let result = {};
    let sanitizedKeys = [];
    if (typeof keys === 'string') {
      sanitizedKeys = [keys];
    } else if (Array.isArray(keys)) {
      sanitizedKeys = keys;
    } else if (keys !== null && typeof keys === 'object') {
      sanitizedKeys = Object.keys(keys);

      // If any key does not exist, use the default value specified in the
      // input.
      result = Object.assign({}, keys);
    } else {
      throw new Error('WebUI localStorageGet() cannot be run with ' + keys);
    }

    for (const key of sanitizedKeys) {
      const value = window.localStorage.getItem(key);
      if (value !== null) {
        result[key] = JSON.parse(value);
      } else if (result[key] === undefined) {
        // For key that does not exist and does not have a default value, set it
        // to null.
        result[key] = null;
      }
    }
    return result;
  }

  /** @override */
  async localStorageSet(items) {
    for (const [key, val] of Object.entries(items)) {
      window.localStorage.setItem(key, JSON.stringify(val));
    }
  }

  /** @override */
  async localStorageRemove(items) {
    if (typeof items === 'string') {
      items = [items];
    }
    for (const key of items) {
      window.localStorage.removeItem(key);
    }
  }

  /** @override */
  async localStorageClear() {
    window.localStorage.clear();
  }

  /** @override */
  async getBoard() {
    return window.loadTimeData.getString('board_name');
  }

  /** @override */
  getI18nMessage(name, ...substitutions) {
    return window.loadTimeData.getStringF(name, ...substitutions);
  }

  /** @override */
  addOnLockListener(callback) {
    ChromeHelper.getInstance().addOnLockListener(callback);
  }

  /** @override */
  async isMetricsAndCrashReportingEnabled() {
    return ChromeHelper.getInstance().isMetricsAndCrashReportingEnabled();
  }

  /** @override */
  async openGallery(file) {
    ChromeHelper.getInstance().openFileInGallery(file.name);
  }

  /** @override */
  openInspector(type) {
    // SWA can open the inspector by the hot-keys and there is no need to
    // implement it.
  }

  /** @override */
  getAppVersion() {
    return 'SWA';
  }

  /** @override */
  getTextDirection() {
    return window.loadTimeData.getString('textdirection');
  }

  /** @override */
  shouldAddFakeHistory() {
    return false;
  }

  /** @override */
  openFeedback() {
    ChromeHelper.getInstance().openFeedbackDialog(
        this.getI18nMessage('feedback_description_placeholder'));
  }

  /** @override */
  async initCameraUsageMonitor(exploitUsage, releaseUsage) {
    return ChromeHelper.getInstance().initCameraUsageMonitor(
        exploitUsage, releaseUsage);
  }

  /** @override */
  setupUnloadListener(listener) {
    window.addEventListener('unload', listener);
  }

  /** @override */
  async setLaunchingFromWindowCreationStartTime(callback) {
    await callback();
  }

  /** @override */
  getUntrustedOrigin() {
    return UntrustedOrigin.CHROME_UNTRUSTED;
  }

  /** @override */
  setBeforeUnloadListenerEnabled(enabled) {
    if (enabled) {
      window.addEventListener('beforeunload', beforeUnloadListener);
    } else {
      window.removeEventListener('beforeunload', beforeUnloadListener);
    }
  }
}

export const browserProxy = new WebUIBrowserProxy();

/* eslint-enable new-cap */
