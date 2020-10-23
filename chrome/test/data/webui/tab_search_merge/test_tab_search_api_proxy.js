// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TabSearchApiProxy} from 'chrome://tab-search/tab_search_api_proxy.js';
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

    /** @type {!tabSearch.mojom.PageCallbackRouter} */
    this.callbackRouter = new tabSearch.mojom.PageCallbackRouter();

    /** @type {!tabSearch.mojom.PageRemote} */
    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();

    /** @private {tabSearch.mojom.ProfileTabs} */
    this.profileTabs_;
  }

  /** @override */
  closeTab(tabId) {
    this.methodCalled('closeTab', tabId);
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
  switchToTab(tabInfo, withSearch) {
    this.methodCalled('switchToTab', [ tabInfo, withSearch ]);
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

  /** return {!tabSearch.mojom.PageRemote} */
  getCallbackRouterRemote() {
    return this.callbackRouterRemote;
  }

  /** @param {tabSearch.mojom.ProfileTabs} profileTabs */
  setProfileTabs(profileTabs) {
    this.profileTabs_ = profileTabs;
  }
}
