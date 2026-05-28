// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PersonalContextNoticeBrowserProxy} from 'chrome://accessibility-annotator-info/browser_proxy.js';
import type {AccountInfo, PageHandlerInterface} from 'chrome://accessibility-annotator-info/personal_context_notice.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

class TestPersonalContextNoticePageHandler extends TestBrowserProxy implements
    PageHandlerInterface {
  private accountInfo_: AccountInfo|null = null;

  constructor() {
    super([
      'getAccountInfo',
      'onInfoAcknowledged',
      'onInfoDismissed',
      'onManageSettingsClicked',
      'onLearnMoreClicked',
      'showUi',
    ]);
  }

  getAccountInfo() {
    this.methodCalled('getAccountInfo');
    return Promise.resolve({info: this.accountInfo_});
  }

  onInfoAcknowledged() {
    this.methodCalled('onInfoAcknowledged');
  }

  onInfoDismissed() {
    this.methodCalled('onInfoDismissed');
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
