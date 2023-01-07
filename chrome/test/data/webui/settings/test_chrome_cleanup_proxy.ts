// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ChromeCleanupProxy} from 'chrome://settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestChromeCleanupProxy extends TestBrowserProxy implements
    ChromeCleanupProxy {
  constructor() {
    super([
      'registerChromeCleanerObserver',
      'restartComputer',
      'startCleanup',
      'startScanning',
      'notifyShowDetails',
      'notifyLearnMoreClicked',
      'getMoreItemsPluralString',
      'getItemsToRemovePluralString',
    ]);
  }

  registerChromeCleanerObserver() {
    this.methodCalled('registerChromeCleanerObserver');
  }

  restartComputer() {
    this.methodCalled('restartComputer');
  }

  startCleanup(logsUploadEnabled: boolean) {
    this.methodCalled('startCleanup', logsUploadEnabled);
  }

  startScanning(logsUploadEnabled: boolean) {
    this.methodCalled('startScanning', logsUploadEnabled);
  }

  notifyShowDetails(enabled: boolean) {
    this.methodCalled('notifyShowDetails', enabled);
  }

  notifyLearnMoreClicked() {
    this.methodCalled('notifyLearnMoreClicked');
  }

  getMoreItemsPluralString(numHiddenItems: number) {
    this.methodCalled('getMoreItemsPluralString', numHiddenItems);
    return Promise.resolve('');
  }

  getItemsToRemovePluralString(numItems: number) {
    this.methodCalled('getItemsToRemovePluralString', numItems);
    return Promise.resolve('');
  }
}
