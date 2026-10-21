// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ForeignSession, ForeignSessionPageHandlerInterface} from 'chrome://resources/cr_components/history/foreign_sessions.mojom-webui.js';
import type {ClickModifiers} from 'chrome://resources/mojo/ui/base/mojom/window_open_disposition.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class FakeForeignSessionPageHandler extends TestBrowserProxy implements
    ForeignSessionPageHandlerInterface {
  private sessions_: ForeignSession[] = [];

  constructor() {
    super([
      'getForeignSessions',
      'openForeignSessionAllTabs',
      'openForeignSessionTab',
      'deleteForeignSession',
      'setForeignSessionCollapsed',
      'showUi',
    ]);
  }

  setForeignSessions(sessions: ForeignSession[]) {
    this.sessions_ = sessions;
  }

  getForeignSessions() {
    this.methodCalled('getForeignSessions');
    return Promise.resolve({sessions: structuredClone(this.sessions_)});
  }

  openForeignSessionAllTabs(sessionTag: string) {
    this.methodCalled('openForeignSessionAllTabs', sessionTag);
  }

  openForeignSessionTab(
      sessionTag: string, tabId: number, modifiers: ClickModifiers) {
    this.methodCalled('openForeignSessionTab', sessionTag, tabId, modifiers);
  }

  deleteForeignSession(sessionTag: string) {
    this.methodCalled('deleteForeignSession', sessionTag);
  }

  setForeignSessionCollapsed(sessionTag: string, collapsed: boolean) {
    this.methodCalled('setForeignSessionCollapsed', sessionTag, collapsed);
  }

  showUi() {
    this.methodCalled('showUi');
  }
}
