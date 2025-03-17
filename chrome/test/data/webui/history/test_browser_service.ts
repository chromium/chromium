// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BrowserService, ForeignSession} from 'chrome://history/history.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/history/history.mojom-webui.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {createHistoryInfo} from './test_util.js';

export class TestBrowserService extends TestBrowserProxy implements
    BrowserService {
  handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  callbackRouter: PageCallbackRouter;
  histogramMap: {[key: string]: {[key: string]: number}} = {};
  actionMap: {[key: string]: number} = {};
  private foreignSessions_: ForeignSession[] = [];

  constructor() {
    super([
      'deleteForeignSession',
      'getForeignSessions',
      'historyLoaded',
      'navigateToUrl',
      'openForeignSessionTab',
      'otherDevicesInitialized',
      'recordHistogram',
      'recordLongTime',
      'startTurnOnSyncFlow',
    ]);

    this.handler = TestMock.fromClass(PageHandlerRemote);
    this.callbackRouter = new PageCallbackRouter();

    this.handler.setResultFor('queryHistory', Promise.resolve({
      results: {
        info: createHistoryInfo(''),
        value: [],
      },
    }));
  }


  deleteForeignSession(sessionTag: string) {
    this.methodCalled('deleteForeignSession', sessionTag);
  }

  getForeignSessions() {
    this.methodCalled('getForeignSessions');
    return Promise.resolve(this.foreignSessions_);
  }

  setForeignSessions(sessions: ForeignSession[]) {
    this.foreignSessions_ = sessions;
  }

  historyLoaded() {
    this.methodCalled('historyLoaded');
  }

  navigateToUrl(url: string, _target: string, _e: MouseEvent) {
    this.methodCalled('navigateToUrl', url);
  }

  openForeignSessionAllTabs() {}

  openForeignSessionTab(sessionTag: string, tabId: number, e: MouseEvent) {
    this.methodCalled('openForeignSessionTab', {
      sessionTag: sessionTag,
      tabId: tabId,
      e: e,
    });
  }

  otherDevicesInitialized() {
    this.methodCalled('otherDevicesInitialized');
  }
  recordAction(action: string) {
    if (!(action in this.actionMap)) {
      this.actionMap[action] = 0;
    }

    this.actionMap[action]!++;
  }

  recordHistogram(histogram: string, value: number, max: number) {
    assertTrue(value <= max);

    if (!(histogram in this.histogramMap)) {
      this.histogramMap[histogram] = {};
    }

    if (!(value in this.histogramMap[histogram]!)) {
      this.histogramMap[histogram]![value] = 0;
    }

    this.histogramMap[histogram]![value]!++;
    this.methodCalled('recordHistogram');
  }

  recordTime() {}

  recordLongTime(histogram: string, value: number) {
    this.methodCalled('recordLongTime', histogram, value);
  }

  removeBookmark() {}
  startTurnOnSyncFlow() {}
}
