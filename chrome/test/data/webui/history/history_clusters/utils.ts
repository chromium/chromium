// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ClusterAction, MetricsProxy, RelatedSearchAction, VisitAction, VisitType} from 'chrome://history/history.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://history/history.js';
import {TestBrowserProxy as BaseTestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

export class TestBrowserProxy extends BaseTestBrowserProxy {
  handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  callbackRouter: PageCallbackRouter;

  constructor() {
    super([]);
    this.handler = TestMock.fromClass(PageHandlerRemote);
    this.callbackRouter = new PageCallbackRouter();
  }
}

export class TestMetricsProxy extends BaseTestBrowserProxy implements
    MetricsProxy {
  constructor() {
    super([
      'recordClusterAction',
      'recordRelatedSearchAction',
      'recordToggledVisibility',
      'recordVisitAction',
    ]);
  }

  recordClusterAction(action: ClusterAction, index: number) {
    this.methodCalled('recordClusterAction', [action, index]);
  }

  recordRelatedSearchAction(action: RelatedSearchAction, index: number) {
    this.methodCalled('recordRelatedSearchAction', [action, index]);
  }

  recordToggledVisibility(visible: boolean) {
    this.methodCalled('recordToggledVisibility', visible);
  }

  recordVisitAction(action: VisitAction, index: number, type: VisitType) {
    this.methodCalled('recordVisitAction', [action, index, type]);
  }
}
