// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ScanningBrowserProxy, SelectedPath} from 'chrome://scanning/scanning_browser_proxy.js';

import {assertEquals} from '../../chai_assert.js';
import {TestBrowserProxy} from '../../test_browser_proxy.m.js';

/**
 * Test version of ScanningBrowserProxy.
 * @implements {ScanningBrowserProxy}
 */
export class TestScanningBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'initialize',
      'requestScanToLocation',
      'showFileInLocation',
      'getPluralString',
    ]);

    /** @private {?SelectedPath} */
    this.selectedPath_ = null;

    /** @private {?string} */
    this.pathToFile_ = null;
  }

  /** @override */
  initialize() {
    this.methodCalled('initialize');
  }

  /**
   * @return {!Promise}
   * @override
   */
  requestScanToLocation() {
    this.methodCalled('requestScanToLocation');
    return Promise.resolve(this.selectedPath_);
  }

  /** @param {string} pathToFile */
  showFileInLocation(pathToFile) {
    this.methodCalled('showFileInLocation');
    return Promise.resolve(this.pathToFile_ === pathToFile);
  }

  /**
   * @param {string} name
   * @param {number} count
   */
  getPluralString(name, count) {
    this.methodCalled('getPluralString');
    return Promise.resolve(
        count === 1 ? 'Scanned file saved!' : 'Scanned files saved!');
  }

  /** @param {!SelectedPath} selectedPath */
  setSelectedPath(selectedPath) {
    this.selectedPath_ = selectedPath;
  }

  /** @param {string} pathToFile */
  setPathToFile(pathToFile) {
    this.pathToFile_ = pathToFile;
  }
}
