// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AccountInfo, PageHandlerInterface} from 'chrome://accessibility-annotator-info/accessibility_annotator_info.mojom-webui.js';
import type {AccessibilityAnnotatorInfoBrowserProxy} from 'chrome://accessibility-annotator-info/browser_proxy.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

class TestAccessibilityAnnotatorInfoPageHandler extends TestBrowserProxy
    implements PageHandlerInterface {
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

export class TestAccessibilityAnnotatorInfoBrowserProxy implements
    AccessibilityAnnotatorInfoBrowserProxy {
  handler: TestAccessibilityAnnotatorInfoPageHandler =
      new TestAccessibilityAnnotatorInfoPageHandler();
}
