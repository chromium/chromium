// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {ScanningBrowserProxy, SelectedPath} from 'chrome://scanning/scanning_browser_proxy.js';
import {TestBrowserProxy} from 'chrome://webui-test/chromeos/test_browser_proxy.js';

const EMPTY_SELECTED_PATH: SelectedPath = {
  baseName: '',
  filePath: '',
};

/**
 * Test version of ScanningBrowserProxy.
 */
export class TestScanningBrowserProxy extends TestBrowserProxy implements
    ScanningBrowserProxy {
  selectedPath = EMPTY_SELECTED_PATH;
  pathToFile: string|null = null;
  myFilesPath = '';
  savedSettings = '';
  savedSettingsSelectedPath = EMPTY_SELECTED_PATH;

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
  }

  initialize(): void {}

  requestScanToLocation(): Promise<SelectedPath> {
    return Promise.resolve(this.selectedPath);
  }

  showFileInLocation(pathToFile: string): Promise<boolean> {
    this.methodCalled('showFileInLocation', pathToFile);
    return Promise.resolve(this.pathToFile === pathToFile);
  }

  getPluralString(name: string, count: number): Promise<string> {
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
    return Promise.resolve(
        `${sanitizeInnerHtml(pluralString, {attrs: ['id']})}`);
  }

  recordScanJobSettings(): void {}

  getMyFilesPath(): Promise<string> {
    return Promise.resolve(this.myFilesPath);
  }

  openFilesInMediaApp(filePaths: string[]): void {
    this.methodCalled('openFilesInMediaApp', filePaths);
  }

  recordScanCompleteAction(): void {}

  recordNumScanSettingChanges(numChanges: number): void {
    this.methodCalled('recordNumScanSettingChanges', numChanges);
  }

  saveScanSettings(scanSettings: string): void {
    this.methodCalled('saveScanSettings', scanSettings);
  }

  getScanSettings(): Promise<string> {
    return Promise.resolve(this.savedSettings);
  }

  ensureValidFilePath(filePath: string): Promise<SelectedPath> {
    return Promise.resolve(
        filePath === this.savedSettingsSelectedPath.filePath ?
            this.savedSettingsSelectedPath :
            EMPTY_SELECTED_PATH);
  }

  recordNumCompletedScans(): void {}

  setSelectedPath(selectedPath: SelectedPath): void {
    this.selectedPath = selectedPath;
  }

  setPathToFile(pathToFile: string): void {
    this.pathToFile = pathToFile;
  }

  setMyFilesPath(myFilesPath: string): void {
    this.myFilesPath = myFilesPath;
  }

  setSavedSettings(savedSettings: string): void {
    this.savedSettings = savedSettings;
  }

  setSavedSettingsSelectedPath(selectedPath: SelectedPath): void {
    this.savedSettingsSelectedPath = selectedPath;
  }
}
