// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/chromeos/test_browser_proxy.js';

const EMPTY_SELECTED_PATH = {
  baseName: '',
  filePath: '',
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
    switch (name) {
      case ('fileSavedText'):
        pluralString = count === 1 ?
            'Your file has been successfully scanned and saved to ' +
                '<a id="folderLink">$1</a>.' :
            'Your files have been successfully scanned and saved to ' +
                '<a id="folderLink">$1</a>.';
        break;
      case ('editButtonLabel'):
        pluralString = count === 1 ? 'Edit file' : 'Edit files';
        break;
      case ('scanButtonText'):
        pluralString = count === 0 ? 'Scan' : 'Scan page ' + count;
        break;
      case ('removePageDialogTitle'):
        pluralString =
            count === 0 ? 'Remove page?' : 'Remove page ' + count + '?';
        break;
      case ('rescanPageDialogTitle'):
        pluralString =
            count === 0 ? 'Rescan page?' : 'Rescan page ' + count + '?';
        break;
    }

    return Promise.resolve(sanitizeInnerHtml(pluralString, {attrs: ['id']}));
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
