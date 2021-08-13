// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isMac, webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {UpdateStatus} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from '../test_browser_proxy.js';

/** @implements {AboutPageBrowserProxy} */
export class TestAboutPageBrowserProxy extends TestBrowserProxy {
  constructor() {
    const methodNames = [
      'pageReady',
      'refreshUpdateStatus',
      'openHelpPage',
      'openFeedbackDialog',
    ];

    if (isMac) {
      methodNames.push('promoteUpdater');
    }

    super(methodNames);

    /** @private {!UpdateStatus} */
    this.updateStatus_ = UpdateStatus.UPDATED;
  }

  /** @param {!UpdateStatus} updateStatus */
  setUpdateStatus(updateStatus) {
    this.updateStatus_ = updateStatus;
  }

  /** @override */
  pageReady() {
    this.methodCalled('pageReady');
  }

  /** @override */
  refreshUpdateStatus() {
    webUIListenerCallback('update-status-changed', {
      progress: 1,
      status: this.updateStatus_,
    });
    this.methodCalled('refreshUpdateStatus');
  }

  /** @override */
  openFeedbackDialog() {
    this.methodCalled('openFeedbackDialog');
  }

  /** @override */
  openHelpPage() {
    this.methodCalled('openHelpPage');
  }
}

if (isMac) {
  /** @override */
  TestAboutPageBrowserProxy.prototype.promoteUpdater = function() {
    this.methodCalled('promoteUpdater');
  };
}
