// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BrowserProxy, HistoryIdentityState} from 'chrome://history/history.js';
import {HistoryPageCallbackRouter, HistoryPageHandlerRemote, HistorySignInState, OpenConversationResult, SyncState} from 'chrome://history/history.js';
import type {PageRemote, TimePeriodOption} from 'chrome://resources/cr_components/history/history.mojom-webui.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy as BaseTestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {createHistoryInfo} from './test_util.js';

export class TestHistoryBrowserProxy extends BaseTestBrowserProxy implements
    BrowserProxy {
  handler: TestMock<HistoryPageHandlerRemote>&HistoryPageHandlerRemote;
  callbackRouter: HistoryPageCallbackRouter;
  pageRemote: PageRemote;
  histogramMap: {[key: string]: {[key: string]: number}} = {};
  actionMap: {[key: string]: number} = {};

  constructor() {
    super([
      'recordAction',
      'recordBooleanHistogram',
      'recordHistogram',
      'recordLongTime',
      'recordTime',
    ]);

    this.handler = TestMock.fromClass(HistoryPageHandlerRemote);
    this.callbackRouter = new HistoryPageCallbackRouter();
    this.pageRemote = this.callbackRouter.$.bindNewPipeAndPassRemote();

    this.setInitialIdentityState({
      signIn: HistorySignInState.kSignedOut,
      tabsSync: SyncState.kTurnedOff,
      historySync: SyncState.kTurnedOff,
    });

    this.handler.setResultFor('queryHistory', Promise.resolve({
      results: {
        info: createHistoryInfo(''),
        value: [],
      },
    }));

    // Pages that don't fill the viewport request more results without
    // scrolling, so provide a default empty, finished continuation.
    this.handler.setResultFor('queryHistoryContinuation', Promise.resolve({
      results: {
        info: createHistoryInfo(''),
        value: [],
      },
    }));

    this.handler.setResultFor('requestAccountInfo', Promise.resolve({
      accountInfo: {
        name: 'Test User',
        email: 'test@google.com',
        accountImageSrc: 'http://example.com/image.png',
      },
    }));

    this.handler.setResultFor(
        'openCriticalActionConversation', Promise.resolve({
          result: OpenConversationResult.kSuccess,
        }));

    this.handler.setResultFor('getSyncedDevicesForFiltering', Promise.resolve({
      devices: [],
    }));

    this.handler.setResultFor(
        'getTimePeriodOptions', Promise.resolve({options: []}));

    // <if expr="not is_chromeos">
    this.handler.setResultFor(
        'shouldShowHistoryPageHistorySyncPromo', Promise.resolve({
          shouldShow: false,
        }));
    // </if>
  }

  setInitialIdentityState(identityState: HistoryIdentityState) {
    this.handler.setResultFor(
        'getInitialIdentityState', Promise.resolve({identityState}));
  }

  recordAction(action: string) {
    if (!(action in this.actionMap)) {
      this.actionMap[action] = 0;
    }

    this.actionMap[action]!++;

    this.methodCalled('recordAction', action);
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

  recordTime(histogram: string, value: number) {
    this.methodCalled('recordTime', [histogram, value]);
  }

  recordLongTime(histogram: string, value: number) {
    this.methodCalled('recordLongTime', histogram, value);
  }

  getTimePeriodOptions(): Promise<{options: TimePeriodOption[]}> {
    return this.handler.getTimePeriodOptions();
  }
}
