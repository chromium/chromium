// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isMac, webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {AboutPageBrowserProxy, UpdateStatus} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from '../test_browser_proxy.m.js';

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

  sendStatusNoInternet() {
    webUIListenerCallback('update-status-changed', {
      progress: 0,
      status: UpdateStatus.FAILED,
      message: 'offline',
      connectionTypes: 'no internet',
    });
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

  /** @override */
  launchReleaseNotes() {}

  /** @override */
  openOsHelpPage() {}

  /** @override */
  requestUpdate() {}

  /** @override */
  requestUpdateOverCellular() {}

  /** @override */
  setChannel() {}

  /** @override */
  getChannelInfo() {}

  /** @override */
  canChangeChannel() {}

  /** @override */
  getVersionInfo() {}


  /** @override */
  getRegulatoryInfo() {}

  /** @override */
  getEndOfLifeInfo() {}

  /** @override */
  checkInternetConnection() {}

  /** @override */
  refreshTPMFirmwareUpdateStatus() {}
}

if (isMac) {
  /** @override */
  TestAboutPageBrowserProxy.prototype.promoteUpdater = function() {
    this.methodCalled('promoteUpdater');
  };
}
