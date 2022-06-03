// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {ClearBrowsingDataBrowserProxy, ClearBrowsingDataResult, InstalledApp} from 'chrome://settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

// clang-format on

export class TestClearBrowsingDataBrowserProxy extends TestBrowserProxy
    implements ClearBrowsingDataBrowserProxy {
  private clearBrowsingDataPromise_: Promise<ClearBrowsingDataResult>|null;
  private installedApps_: InstalledApp[];

  constructor() {
    super(['initialize', 'clearBrowsingData', 'getInstalledApps']);

    /**
     * The promise to return from |clearBrowsingData|.
     * Allows testing code to test what happens after the call is made, and
     * before the browser responds.
     */
    this.clearBrowsingDataPromise_ = null;

    /**
     * Response for |getInstalledApps|.
     */
    this.installedApps_ = [];
  }

  setClearBrowsingDataPromise(promise: Promise<ClearBrowsingDataResult>) {
    this.clearBrowsingDataPromise_ = promise;
  }

  clearBrowsingData(
      dataTypes: Array<string>, timePeriod: number,
      installedApps: Array<InstalledApp>) {
    this.methodCalled(
        'clearBrowsingData', [dataTypes, timePeriod, installedApps]);
    webUIListenerCallback('browsing-data-removing', true);
    return this.clearBrowsingDataPromise_ !== null ?
        this.clearBrowsingDataPromise_ :
        Promise.resolve({showHistoryNotice: false, showPasswordsNotice: false});
  }

  setInstalledApps(apps: InstalledApp[]) {
    this.installedApps_ = apps;
  }

  getInstalledApps(timePeriod: number) {
    this.methodCalled('getInstalledApps', timePeriod);
    return Promise.resolve(this.installedApps_);
  }

  initialize() {
    this.methodCalled('initialize');
    return Promise.resolve();
  }
}
