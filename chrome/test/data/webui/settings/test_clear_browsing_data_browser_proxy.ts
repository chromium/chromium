// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {ClearBrowsingDataBrowserProxy, ClearBrowsingDataResult, TimePeriodExperiment, UpdateSyncStateEvent} from 'chrome://settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

// clang-format on

export class TestClearBrowsingDataBrowserProxy extends TestBrowserProxy
    implements ClearBrowsingDataBrowserProxy {
  private clearBrowsingDataPromise_: Promise<ClearBrowsingDataResult>|null;

  constructor() {
    super([
      'initialize',
      'clearBrowsingData',
      'restartCounters',
      'recordSettingsClearBrowsingDataBasicTimePeriodHistogram',
      'recordSettingsClearBrowsingDataAdvancedTimePeriodHistogram',
    ]);

    /**
     * The promise to return from |clearBrowsingData|.
     * Allows testing code to test what happens after the call is made, and
     * before the browser responds.
     */
    this.clearBrowsingDataPromise_ = null;
  }

  setClearBrowsingDataPromise(promise: Promise<ClearBrowsingDataResult>) {
    this.clearBrowsingDataPromise_ = promise;
  }

  clearBrowsingData(dataTypes: string[], timePeriod: number) {
    this.methodCalled('clearBrowsingData', [dataTypes, timePeriod]);
    webUIListenerCallback('browsing-data-removing', true);
    return this.clearBrowsingDataPromise_ !== null ?
        this.clearBrowsingDataPromise_ :
        Promise.resolve({showHistoryNotice: false, showPasswordsNotice: false});
  }

  initialize() {
    this.methodCalled('initialize');
    return Promise.resolve();
  }

  getSyncState(): Promise<UpdateSyncStateEvent> {
    this.methodCalled('getSyncState');
    return Promise.resolve({
      signedIn: false,
      syncConsented: false,
      syncingHistory: false,
      shouldShowCookieException: false,
      isNonGoogleDse: false,
      nonGoogleSearchHistoryString: 'somestring',
    });
  }

  restartCounters(isBasic: boolean, timePeriod: number) {
    this.methodCalled('restartCounters', isBasic, timePeriod);
    return Promise.resolve();
  }

  recordSettingsClearBrowsingDataBasicTimePeriodHistogram(
      bucket: TimePeriodExperiment) {
    this.methodCalled(
        'recordSettingsClearBrowsingDataBasicTimePeriodHistogram', bucket);
  }

  recordSettingsClearBrowsingDataAdvancedTimePeriodHistogram(
      bucket: TimePeriodExperiment) {
    this.methodCalled(
        'recordSettingsClearBrowsingDataAdvancedTimePeriodHistogram', bucket);
  }
}
