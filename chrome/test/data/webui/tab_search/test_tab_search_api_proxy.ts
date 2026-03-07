// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageRemote, ProfileData, SwitchToTabInfo, TabSearchApiProxy} from 'chrome://tab-search.top-chrome/tab_search.js';
import {PageCallbackRouter, TabSearchSection} from 'chrome://tab-search.top-chrome/tab_search.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestTabSearchApiProxy extends TestBrowserProxy implements
    TabSearchApiProxy {
  callbackRouter: PageCallbackRouter;
  callbackRouterRemote: PageRemote;
  private profileData_?: ProfileData;
  private isSplit_: boolean = false;

  constructor() {
    super([
      'closeTab',
      'closeWebUiTab',
      'getProfileData',
      'getTabSearchSection',
      'getIsSplit',
      'openRecentlyClosedEntry',
      'replaceActiveSplitTab',
      'switchToTab',
      'saveRecentlyClosedExpandedPref',
      'maybeShowUi',
    ]);

    this.callbackRouter = new PageCallbackRouter();

    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();
  }

  closeTab(tabId: number) {
    this.methodCalled('closeTab', [tabId]);
  }

  closeWebUiTab() {
    this.methodCalled('closeWebUiTab', []);
  }

  getProfileData() {
    this.methodCalled('getProfileData');
    return Promise.resolve({profileData: this.profileData_!});
  }

  getTabSearchSection() {
    this.methodCalled('getTabSearchSection');
    return Promise.resolve({section: TabSearchSection.kSearch});
  }

  getIsSplit() {
    this.methodCalled('getIsSplit');
    return Promise.resolve({isSplit: this.isSplit_});
  }

  openRecentlyClosedEntry(
      id: number, withSearch: boolean, isTab: boolean, index: number) {
    this.methodCalled(
        'openRecentlyClosedEntry', [id, withSearch, isTab, index]);
  }

  replaceActiveSplitTab(replacementTabId: number) {
    this.methodCalled('replaceActiveSplitTab', [replacementTabId]);
  }

  switchToTab(info: SwitchToTabInfo) {
    this.methodCalled('switchToTab', [info]);
  }

  saveRecentlyClosedExpandedPref(expanded: boolean) {
    this.methodCalled('saveRecentlyClosedExpandedPref', [expanded]);
  }

  maybeShowUi() {
    this.methodCalled('maybeShowUi');
  }

  getCallbackRouter() {
    return this.callbackRouter;
  }

  getCallbackRouterRemote() {
    return this.callbackRouterRemote;
  }

  setProfileData(profileData: ProfileData) {
    this.profileData_ = profileData;
  }

  setIsSplit(isSplit: boolean) {
    this.isSplit_ = isSplit;
  }
}
