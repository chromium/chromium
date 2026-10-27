// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageHandlerInterface, ProfileData, SwitchToTabInfo, TabSearchApiProxy} from 'chrome://tab-search.top-chrome/tab_search.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestTabSearchApiProxy extends TestBrowserProxy implements
    PageHandlerInterface, TabSearchApiProxy {
  private profileData_?: ProfileData;
  private isSplit_: boolean = false;

  constructor() {
    super([
      'closeTab',
      'closeTabs',
      'closeWebUiTab',
      'getProfileData',
      'getIsSplit',
      'openRecentlyClosedEntry',
      'recordRecentlyClosedItemOpenAction',
      'replaceActiveSplitTab',
      'switchToTab',
      'saveRecentlyClosedExpandedPref',
      'startTabGroupTutorial',
      'maybeShowUI',
      'redirectToNtp',
    ]);
  }

  closeTab(tabId: number) {
    this.methodCalled('closeTab', [tabId]);
  }

  closeTabs(tabIds: number[]) {
    this.methodCalled('closeTabs', [tabIds]);
  }

  closeWebUiTab() {
    this.methodCalled('closeWebUiTab', []);
  }

  getProfileData() {
    this.methodCalled('getProfileData');
    return Promise.resolve({profileData: this.profileData_!});
  }

  getIsSplit() {
    this.methodCalled('getIsSplit');
    return Promise.resolve({isSplit: this.isSplit_});
  }

  openRecentlyClosedEntry(id: number) {
    this.methodCalled('openRecentlyClosedEntry', [id]);
  }

  recordRecentlyClosedItemOpenAction(withSearch: boolean, isTab: boolean) {
    this.methodCalled(
        'recordRecentlyClosedItemOpenAction', [withSearch, isTab]);
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

  startTabGroupTutorial() {
    this.methodCalled('startTabGroupTutorial');
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  maybeShowUI() {
    this.methodCalled('maybeShowUI');
  }

  redirectToNtp() {
    this.methodCalled('redirectToNtp');
  }

  setProfileData(profileData: ProfileData) {
    this.profileData_ = profileData;
  }

  setIsSplit(isSplit: boolean) {
    this.isSplit_ = isSplit;
  }
}
