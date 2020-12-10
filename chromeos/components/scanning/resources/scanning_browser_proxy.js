// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the Scanning App UI in chromeos/ to
 * provide access to the ScanningHandler which invokes functions that only exist
 * in chrome/.
 */

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

/**
 * @typedef {{
 *   baseName: string,
 *   filePath: string,
 * }}
 */
export let SelectedPath;

/** @interface */
export class ScanningBrowserProxy {
  /** Initialize ScanningHandler. */
  initialize() {}

  /**
   * Requests the user to choose the directory to save scans.
   * @return {!Promise<!SelectedPath>}
   */
  requestScanToLocation() {}

  /**
   * Opens the Files app with the file |pathToFile| highlighted.
   * @param {string} pathToFile
   * @return {!Promise<boolean>} True if the file is found and Files app opens.
   */
  showFileInLocation(pathToFile) {}

  /**
   * Returns a localized, pluralized string for |name| based on |count|.
   * @param {string} name
   * @param {number} count
   * @return {!Promise<string>}
   */
  getPluralString(name, count) {}
}

/** @implements {ScanningBrowserProxy} */
export class ScanningBrowserProxyImpl {
  /** @override */
  initialize() {
    chrome.send('initialize');
  }

  /** @override */
  requestScanToLocation() {
    return sendWithPromise('requestScanToLocation');
  }

  /** @override */
  showFileInLocation(pathToFile) {
    return sendWithPromise('showFileInLocation', pathToFile);
  }

  /** @override */
  getPluralString(name, count) {
    return sendWithPromise('getPluralString', name, count);
  }
}

// The singleton instance_ can be replaced with a test version of this wrapper
// during testing.
addSingletonGetter(ScanningBrowserProxyImpl);
