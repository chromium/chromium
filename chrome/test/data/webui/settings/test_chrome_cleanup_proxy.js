// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.js';

/** @implements {ChromeCleanupProxy} */
export class TestChromeCleanupProxy extends TestBrowserProxy {
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

  /** @override */
  registerChromeCleanerObserver() {
    this.methodCalled('registerChromeCleanerObserver');
  }

  /** @override */
  restartComputer() {
    this.methodCalled('restartComputer');
  }

  /** @override */
  startCleanup(logsUploadEnabled) {
    this.methodCalled('startCleanup', logsUploadEnabled);
  }

  /** @override */
  startScanning(logsUploadEnabled) {
    this.methodCalled('startScanning', logsUploadEnabled);
  }

  /** @override */
  notifyShowDetails(enabled) {
    this.methodCalled('notifyShowDetails', enabled);
  }

  /** @override */
  notifyLearnMoreClicked() {
    this.methodCalled('notifyLearnMoreClicked');
  }

  /** @override */
  getMoreItemsPluralString(numHiddenItems) {
    this.methodCalled('getMoreItemsPluralString', numHiddenItems);
    return Promise.resolve('');
  }

  /** @override */
  getItemsToRemovePluralString(numItems) {
    this.methodCalled('getItemsToRemovePluralString', numItems);
    return Promise.resolve('');
  }
}
