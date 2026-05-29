// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PersonalContextNoticeBrowserProxy} from 'chrome://personal-context-notice/browser_proxy.js';
import type {AccountInfo, PageHandlerInterface} from 'chrome://personal-context-notice/personal_context_notice.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

class TestPersonalContextNoticePageHandler extends TestBrowserProxy implements
    PageHandlerInterface {
  private accountInfo_: AccountInfo|null = null;

  constructor() {
    super([
      'getAccountInfo',
      'onNoticeAcknowledged',
      'onNoticeDismissed',
      'onManageSettingsClicked',
      'onLearnMoreClicked',
      'showUi',
    ]);
  }

  getAccountInfo() {
    this.methodCalled('getAccountInfo');
    return Promise.resolve({info: this.accountInfo_});
  }

  onNoticeAcknowledged() {
    this.methodCalled('onNoticeAcknowledged');
  }

  onNoticeDismissed() {
    this.methodCalled('onNoticeDismissed');
  }

  onManageSettingsClicked() {
    this.methodCalled('onManageSettingsClicked');
  }

  onLearnMoreClicked() {
    this.methodCalled('onLearnMoreClicked');
  }

  showUi() {
    this.methodCalled('showUi');
  }

  setAccountInfo(accountInfo: AccountInfo) {
    this.accountInfo_ = accountInfo;
  }
}

export class TestPersonalContextNoticeBrowserProxy implements
    PersonalContextNoticeBrowserProxy {
  handler: TestPersonalContextNoticePageHandler =
      new TestPersonalContextNoticePageHandler();
}
