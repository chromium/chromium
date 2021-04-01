// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageRemote, ProfileData, TabSearchApiProxy} from 'chrome://tab-search.top-chrome/tab_search.js';

import {TestBrowserProxy} from '../test_browser_proxy.m.js';

/** @implements {TabSearchApiProxy} */
export class TestTabSearchApiProxy extends TestBrowserProxy {
  constructor() {
    super([
      'closeTab',
      'getProfileData',
      'openRecentlyClosedTab',
      'switchToTab',
      'showUI',
      'closeUI',
    ]);

    /** @type {!PageCallbackRouter} */
    this.callbackRouter = new PageCallbackRouter();

    /** @type {!PageRemote} */
    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();

    /** @private {ProfileData} */
    this.profileData_;
  }

  /** @override */
  closeTab(tabId, withSearch, closedTabIndex) {
    this.methodCalled('closeTab', [tabId, withSearch, closedTabIndex]);
  }

  /** @override */
  getProfileData() {
    this.methodCalled('getProfileData');
    return Promise.resolve({profileData: this.profileData_});
  }

  /** @override */
  openRecentlyClosedTab(tabId) {
    this.methodCalled('openRecentlyClosedTab', tabId);
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

  /** @param {ProfileData} profileData */
  setProfileData(profileData) {
    this.profileData_ = profileData;
  }
}
