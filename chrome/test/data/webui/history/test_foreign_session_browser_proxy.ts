// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ForeignSession, ForeignSessionBrowserProxy} from 'chrome://history/history.js';
import {ForeignSessionPageCallbackRouter, ForeignSessionPageHandlerRemote} from 'chrome://resources/cr_components/history/foreign_sessions.mojom-webui.js';
import {TestBrowserProxy as BaseTestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

export class TestHistoryForeignSessionBrowserProxy extends BaseTestBrowserProxy
    implements ForeignSessionBrowserProxy {
  handler: TestMock<ForeignSessionPageHandlerRemote>&
      ForeignSessionPageHandlerRemote;
  callbackRouter: ForeignSessionPageCallbackRouter;

  constructor() {
    super([]);

    this.handler = TestMock.fromClass(ForeignSessionPageHandlerRemote);
    this.callbackRouter = new ForeignSessionPageCallbackRouter();
    this.handler.setResultFor(
        'getForeignSessions', Promise.resolve({sessions: []}));
  }

  setForeignSessions(sessions: ForeignSession[]) {
    this.handler.setResultFor(
        'getForeignSessions', Promise.resolve({sessions}));
  }
}
