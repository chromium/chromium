// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {createHistoryInfo} from 'chrome://test/history/test_util.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';

export class TestBrowserService extends TestBrowserProxy {
  constructor() {
    super([
      'deleteForeignSession',
      'getForeignSessions',
      'historyLoaded',
      'navigateToUrl',
      'openForeignSessionTab',
      'otherDevicesInitialized',
      'recordHistogram',
      'removeVisits',
      'queryHistory',
      'queryHistoryContinuation',
    ]);
    this.histogramMap = {};
    this.actionMap = {};
    /** @private {?PromiseResolver} */
    this.delayedRemove_ = null;
    /** @private {?PromiseResolver} */
    this.delayedQueryResult_ = null;
    this.ignoreNextQuery_ = false;
    /** @private {!Array<!ForeignSession>} */
    this.foreignSessions_ = [];
    /** @private {!{info: !HistoryQuery, value: !Array<!HistoryEntry>}} */
    this.queryResult_ = {info: createHistoryInfo(), value: []};
  }

  // Will delay resolution of the queryHistory() promise until
  // finishQueryHistory is called.
  delayQueryResult() {
    this.delayedQueryResult_ = new PromiseResolver();
  }

  // Will delay resolution of the removeVisits() promise until
  // finishRemoveVisits is called.
  delayDelete() {
    this.delayedRemove_ = new PromiseResolver();
  }

  // Prevents a call to methodCalled for the next call to queryHistory.
  ignoreNextQuery() {
    this.ignoreNextQuery_ = true;
  }

  /** @override */
  deleteForeignSession(sessionTag) {
    this.methodCalled('deleteForeignSession', sessionTag);
  }

  /** @override */
  getForeignSessions() {
    this.methodCalled('getForeignSessions');
    return Promise.resolve(this.foreignSessions_);
  }

  /** @param {!Array<!ForeignSession>} sessions */
  setForeignSessions(sessions) {
    this.foreignSessions_ = sessions;
  }

  /** @override */
  removeVisits(visits) {
    this.methodCalled('removeVisits', visits);
    if (this.delayedRemove_) {
      return this.delayedRemove_.promise;
    }
    return Promise.resolve();
  }

  // Resolves the removeVisits promise. delayRemove() must be called first.
  finishRemoveVisits() {
    this.delayedRemove_.resolve();
    this.delayedRemove_ = null;
  }

  // Resolves the queryHistory promise. delayQueryHistory() must be called
  // first.
  finishQueryHistory() {
    this.delayedQueryResult_.resolve(this.queryResult_);
    this.delayedQueryResult_ = null;
  }

  /** @override */
  historyLoaded() {
    this.methodCalled('historyLoaded');
  }

  /** @override */
  navigateToUrl(url, target, e) {
    this.methodCalled('navigateToUrl', url);
  }

  /** @override */
  openClearBrowsingData() {}

  /** @override */
  openForeignSessionAllTabs() {}

  /** @override */
  openForeignSessionTab(sessionTag, windowId, tabId, e) {
    this.methodCalled('openForeignSessionTab', {
      sessionTag: sessionTag,
      windowId: windowId,
      tabId: tabId,
      e: e,
    });
  }

  /** @override */
  otherDevicesInitialized() {
    this.methodCalled('otherDevicesInitialized');
  }

  /** @param {{info: !HistoryQuery, value: !Array<!QueryResult>}} queryResult */
  setQueryResult(queryResult) {
    this.queryResult_ = queryResult;
  }

  /** @override */
  queryHistory(searchTerm) {
    if (!this.ignoreNextQuery_) {
      this.methodCalled('queryHistory', searchTerm);
    } else {
      this.ignoreNextQuery_ = false;
    }
    if (this.delayedQueryResult_) {
      return this.delayedQueryResult_.promise;
    }
    return Promise.resolve(this.queryResult_);
  }

  /** @override */
  queryHistoryContinuation() {
    this.methodCalled('queryHistoryContinuation');
    return Promise.resolve(this.queryResult_);
  }

  /** @override */
  recordAction(action) {
    if (!(action in this.actionMap)) {
      this.actionMap[action] = 0;
    }

    this.actionMap[action]++;
  }

  /** @override */
  recordHistogram(histogram, value, max) {
    assertTrue(value <= max);

    if (!(histogram in this.histogramMap)) {
      this.histogramMap[histogram] = {};
    }

    if (!(value in this.histogramMap[histogram])) {
      this.histogramMap[histogram][value] = 0;
    }

    this.histogramMap[histogram][value]++;
    this.methodCalled('recordHistogram');
  }

  /** @override */
  recordTime() {}

  /** @override */
  removeBookmark() {}
}
