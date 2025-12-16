// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BrowserService, ForeignSession, HistoryIdentityState} from 'chrome://history/history.js';
import {HistorySignInState, SyncState} from 'chrome://history/history.js';
import {
  PageCallbackRouter,
  PageHandlerRemote,
  type PageRemote,
} from 'chrome://resources/cr_components/history/history.mojom-webui.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {createHistoryInfo} from './test_util.js';

export class TestBrowserService extends TestBrowserProxy implements
    BrowserService {
  handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  callbackRouter: PageCallbackRouter;
  pageRemote: PageRemote;
  histogramMap: {[key: string]: {[key: string]: number}} = {};
  actionMap: {[key: string]: number} = {};
  private foreignSessions_: ForeignSession[] = [];
  private initialIdentityState_: HistoryIdentityState;

  constructor() {
    super([
      'deleteForeignSession',
      'getForeignSessions',
      'getInitialIdentityState',
      'historyLoaded',
      'navigateToUrl',
      'openForeignSessionTab',
      'otherDevicesInitialized',
      'recordBooleanHistogram',
      'recordHistogram',
      'recordLongTime',
      'recordSigninPendingOffered',
      'startTurnOnSyncFlow',
    ]);

    this.handler = TestMock.fromClass(PageHandlerRemote);
    this.callbackRouter = new PageCallbackRouter();
    this.pageRemote = this.callbackRouter.$.bindNewPipeAndPassRemote();

    this.initialIdentityState_ = {
      signIn: HistorySignInState.SIGNED_OUT,
      tabsSync: SyncState.TURNED_OFF,
      historySync: SyncState.TURNED_OFF,
    };

    this.handler.setResultFor('queryHistory', Promise.resolve({
      results: {
        info: createHistoryInfo(''),
        value: [],
      },
    }));

    this.handler.setResultFor('requestAccountInfo', Promise.resolve({
      accountInfo: {
        name: 'Test User',
        email: 'test@google.com',
        accountImageSrc: {url: 'http://example.com/image.png'},
      },
    }));

    // <if expr="not is_chromeos">
    this.handler.setResultFor(
        'shouldShowHistoryPageHistorySyncPromo', Promise.resolve({
          shouldShow: false,
        }));
    // </if>
  }


  deleteForeignSession(sessionTag: string) {
    this.methodCalled('deleteForeignSession', sessionTag);
  }

  getForeignSessions() {
    this.methodCalled('getForeignSessions');
    return Promise.resolve(this.foreignSessions_);
  }

  getInitialIdentityState(): Promise<HistoryIdentityState> {
    this.methodCalled('getInitialIdentityState');
    return Promise.resolve(this.initialIdentityState_);
  }

  setInitialIdentityState(identityState: HistoryIdentityState) {
    this.initialIdentityState_ = identityState;
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

  recordBooleanHistogram(histogram: string, value: boolean) {
    this.methodCalled('recordBooleanHistogram', [histogram, value]);
  }

  recordTime() {}

  recordLongTime(histogram: string, value: number) {
    this.methodCalled('recordLongTime', histogram, value);
  }

  recordSigninPendingOffered() {
    this.methodCalled('recordSigninPendingOffered');
  }

  removeBookmark() {}

  startTurnOnSyncFlow() {}
}
