// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {AboutPageBrowserProxy} from 'chrome://settings/settings.js';
import {UpdateStatus} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestAboutPageBrowserProxy extends TestBrowserProxy implements
    AboutPageBrowserProxy {
  private updateStatus_: UpdateStatus = UpdateStatus.UPDATED;

  constructor() {
    super([
      'pageReady', 'refreshUpdateStatus', 'openHelpPage', 'openFeedbackDialog',

      // <if expr="is_macosx">
      'promoteUpdater',
      // </if>
    ]);
  }

  setUpdateStatus(updateStatus: UpdateStatus) {
    this.updateStatus_ = updateStatus;
  }

  pageReady() {
    this.methodCalled('pageReady');
  }

  refreshUpdateStatus() {
    webUIListenerCallback('update-status-changed', {
      progress: 1,
      status: this.updateStatus_,
    });
    this.methodCalled('refreshUpdateStatus');
  }

  openFeedbackDialog() {
    this.methodCalled('openFeedbackDialog');
  }

  openHelpPage() {
    this.methodCalled('openHelpPage');
  }

  // <if expr="is_macosx">
  promoteUpdater() {
    this.methodCalled('promoteUpdater');
  }
  // </if>
}
