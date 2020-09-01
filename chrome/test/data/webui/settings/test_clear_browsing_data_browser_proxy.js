// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {ClearBrowsingDataBrowserProxy, InstalledApp, LanguagesBrowserProxy} from 'chrome://settings/lazy_load.js';

import {TestBrowserProxy} from '../test_browser_proxy.m.js';

// clang-format on

/** @implements {ClearBrowsingDataBrowserProxy} */
export class TestClearBrowsingDataBrowserProxy extends TestBrowserProxy {
  constructor() {
    super(['initialize', 'clearBrowsingData', 'getInstalledApps']);

    /**
     * The promise to return from |clearBrowsingData|.
     * Allows testing code to test what happens after the call is made, and
     * before the browser responds.
     * @private {?Promise}
     */
    this.clearBrowsingDataPromise_ = null;

    /**
     * Response for |getInstalledApps|.
     * @private {!Array<!InstalledApp>}
     */
    this.installedApps_ = [];
  }

  /** @param {!Promise} promise */
  setClearBrowsingDataPromise(promise) {
    this.clearBrowsingDataPromise_ = promise;
  }

  /** @override */
  clearBrowsingData(dataTypes, timePeriod, installedApps) {
    this.methodCalled(
        'clearBrowsingData', [dataTypes, timePeriod, installedApps]);
    webUIListenerCallback('browsing-data-removing', true);
    return this.clearBrowsingDataPromise_ !== null ?
        this.clearBrowsingDataPromise_ :
        Promise.resolve({showHistoryNotice: false, showPasswordsNotice: false});
  }

  /** @param {!Array<!InstalledApp>} apps */
  setInstalledApps(apps) {
    this.installedApps_ = apps;
  }

  /** @override */
  getInstalledApps(timePeriod) {
    this.methodCalled('getInstalledApps');
    return Promise.resolve(this.installedApps_);
  }

  /** @override */
  initialize() {
    this.methodCalled('initialize');
    return Promise.resolve();
  }
}
