// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ScanningBrowserProxy, SelectedPath} from 'chrome://scanning/scanning_browser_proxy.js';

import {assertArrayEquals, assertEquals} from '../../chai_assert.js';
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
      'recordScanJobSettings',
      'getMyFilesPath',
      'openFilesInMediaApp',
      'recordScanCompleteAction',
      'recordNumScanSettingChanges',
    ]);

    /** @private {?SelectedPath} */
    this.selectedPath_ = null;

    /** @private {?string} */
    this.pathToFile_ = null;

    /** @private {string} */
    this.myFilesPath_ = '';

    /** @private {!Array<string>} */
    this.filePaths_ = [];

    /** @private {number} */
    this.expectedNumScanSettingChanges_ = 0;
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

    let pluralString = '';
    if (name === 'fileSavedText') {
      pluralString = count === 1 ?
          'Your file has been successfully scanned and saved to ' +
              '<a id="folderLink">$1</a>.' :
          'Your files have been successfully scanned and saved to ' +
              '<a id="folderLink">$1</a>.';
    } else if (name === 'editButtonLabel') {
      pluralString = count === 1 ? 'Edit file' : 'Edit files';
    }

    return Promise.resolve(pluralString);
  }

  /** @override */
  recordScanJobSettings() {}

  /** @override */
  getMyFilesPath() {
    this.methodCalled('getMyFilesPath');
    return Promise.resolve(this.myFilesPath_);
  }

  /**
   * @param {!Array<string>} filePaths
   * @override
   */
  openFilesInMediaApp(filePaths) {
    this.methodCalled('openFilesInMediaApp');
    assertArrayEquals(this.filePaths_, filePaths);
  }

  /** @override */
  recordScanCompleteAction() {}

  /** @override */
  recordNumScanSettingChanges(numChanges) {
    assertEquals(this.expectedNumScanSettingChanges_, numChanges);
  }

  /** @param {!SelectedPath} selectedPath */
  setSelectedPath(selectedPath) {
    this.selectedPath_ = selectedPath;
  }

  /** @param {string} pathToFile */
  setPathToFile(pathToFile) {
    this.pathToFile_ = pathToFile;
  }

  /** @param {string} myFilesPath */
  setMyFilesPath(myFilesPath) {
    this.myFilesPath_ = myFilesPath;
  }

  /** @param {!Array<string>} filePaths */
  setFilePaths(filePaths) {
    this.filePaths_ = filePaths;
  }

  /** @param {number} numChanges */
  setExpectedNumScanSettingChanges(numChanges) {
    this.expectedNumScanSettingChanges_ = numChanges;
  }
}
