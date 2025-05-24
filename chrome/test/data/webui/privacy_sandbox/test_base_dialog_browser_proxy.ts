// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BaseDialogPageHandlerInterface} from 'chrome://privacy-sandbox-base-dialog/base_dialog.mojom-webui.js';
import {BaseDialogPageCallbackRouter} from 'chrome://privacy-sandbox-base-dialog/base_dialog.mojom-webui.js';
import type {BaseDialogPageRemote} from 'chrome://privacy-sandbox-base-dialog/base_dialog.mojom-webui.js';
import type {PrivacySandboxNotice, PrivacySandboxNoticeEvent} from 'chrome://privacy-sandbox-base-dialog/notice.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestBaseDialogBrowserProxy {
  callbackRouter: BaseDialogPageCallbackRouter =
      new BaseDialogPageCallbackRouter();
  handler: TestBaseDialogPageHandler;
  page: BaseDialogPageRemote;

  constructor() {
    this.handler = new TestBaseDialogPageHandler();
    this.page = this.callbackRouter.$.bindNewPipeAndPassRemote();
  }
}

export class TestBaseDialogPageHandler extends TestBrowserProxy implements
    BaseDialogPageHandlerInterface {
  constructor() {
    super([
      'resizeDialog',
      'showDialog',
      'eventOccurred',
    ]);
  }

  resizeDialog(height: number) {
    this.methodCalled('resizeDialog', height);
  }

  showDialog() {
    this.methodCalled('showDialog');
  }

  eventOccurred(
      notice: PrivacySandboxNotice, event: PrivacySandboxNoticeEvent) {
    this.methodCalled('eventOccurred', notice, event);
  }
}
