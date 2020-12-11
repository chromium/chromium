// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageRemote, ProfileTabs, TabSearchApiProxy} from 'chrome://tab-search/tab_search.js';

import {TestBrowserProxy} from '../test_browser_proxy.m.js';

/** @implements {TabSearchApiProxy} */
export class TestTabSearchApiProxy extends TestBrowserProxy {
  constructor() {
    super([
      'closeTab',
      'getProfileTabs',
      'showFeedbackPage',
      'switchToTab',
      'showUI',
      'closeUI',
    ]);

    /** @type {!PageCallbackRouter} */
    this.callbackRouter = new PageCallbackRouter();

    /** @type {!PageRemote} */
    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();

    /** @private {ProfileTabs} */
    this.profileTabs_;
  }

  /** @override */
  closeTab(tabId, withSearch, closedTabIndex) {
    this.methodCalled('closeTab', [tabId, withSearch, closedTabIndex]);
  }

  /** @override */
  getProfileTabs() {
    this.methodCalled('getProfileTabs');
    return Promise.resolve({profileTabs: this.profileTabs_});
  }

  /** @override */
  showFeedbackPage() {
    this.methodCalled('showFeedbackPage');
  }

  /** @override */
  switchToTab(tabInfo, withSearch, switchedTabIndex) {
    this.methodCalled('switchToTab', [tabInfo, withSearch, switchedTabIndex]);
  }

  /** @override */
  showUI() {
    this.methodCalled('showUI');
  }

  /** @override */
  closeUI() {
    this.methodCalled('closeUI');
  }

  /** @override */
  getCallbackRouter() {
    return this.callbackRouter;
  }

  /** return {!PageRemote} */
  getCallbackRouterRemote() {
    return this.callbackRouterRemote;
  }

  /** @param {ProfileTabs} profileTabs */
  setProfileTabs(profileTabs) {
    this.profileTabs_ = profileTabs;
  }
}
