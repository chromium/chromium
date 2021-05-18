// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ScanningBrowserProxy, SelectedPath} from 'chrome://scanning/scanning_browser_proxy.js';

import {assertEquals} from '../../chai_assert.js';
import {TestBrowserProxy} from '../../test_browser_proxy.m.js';

const EMPTY_SELECTED_PATH = {
  baseName: '',
  filePath: ''
};

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
      'saveScanSettings',
      'getScanSettings',
      'ensureValidFilePath',
      'recordNumCompletedScans',
    ]);

    /** @private {!SelectedPath} */
    this.selectedPath_ = EMPTY_SELECTED_PATH;

    /** @private {?string} */
    this.pathToFile_ = null;

    /** @private {string} */
    this.myFilesPath_ = '';

    /** @private {string} */
    this.savedSettings_ = '';

    /** @private {!SelectedPath} */
    this.savedSettingsSelectedPath_ = EMPTY_SELECTED_PATH;
  }

  /** @override */
  initialize() {}

  /**
   * @return {!Promise}
   * @override
   */
  requestScanToLocation() {
    return Promise.resolve(this.selectedPath_);
  }

  /** @param {string} pathToFile */
  showFileInLocation(pathToFile) {
    this.methodCalled('showFileInLocation', pathToFile);
    return Promise.resolve(this.pathToFile_ === pathToFile);
  }

  /**
   * @param {string} name
   * @param {number} count
   */
  getPluralString(name, count) {
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
    return Promise.resolve(this.myFilesPath_);
  }

  /**
   * @param {!Array<string>} filePaths
   * @override
   */
  openFilesInMediaApp(filePaths) {
    this.methodCalled('openFilesInMediaApp', filePaths);
  }

  /** @override */
  recordScanCompleteAction() {}

  /** @override */
  recordNumScanSettingChanges(numChanges) {
    this.methodCalled('recordNumScanSettingChanges', numChanges);
  }

  /** @override */
  saveScanSettings(scanSettings) {
    this.methodCalled('saveScanSettings', scanSettings);
  }

  /** @override */
  getScanSettings() {
    return Promise.resolve(this.savedSettings_);
  }

  /** @override */
  ensureValidFilePath(filePath) {
    return Promise.resolve(
        filePath === this.savedSettingsSelectedPath_.filePath ?
            this.savedSettingsSelectedPath_ :
            EMPTY_SELECTED_PATH);
  }

  /** @override */
  recordNumCompletedScans() {}

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

  /** @param {string} savedSettings */
  setSavedSettings(savedSettings) {
    this.savedSettings_ = savedSettings;
  }

  /** @param {!SelectedPath} selectedPath */
  setSavedSettingsSelectedPath(selectedPath) {
    this.savedSettingsSelectedPath_ = selectedPath;
  }
}
