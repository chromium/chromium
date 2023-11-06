// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageRemote, ProfileData, SwitchToTabInfo, Tab, TabOrganizationSession, TabSearchApiProxy} from 'chrome://tab-search.top-chrome/tab_search.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestTabSearchApiProxy extends TestBrowserProxy implements
    TabSearchApiProxy {
  callbackRouter: PageCallbackRouter;
  callbackRouterRemote: PageRemote;
  private profileData_?: ProfileData;
  private tabOrganizationSession_?: TabOrganizationSession;

  constructor() {
    super([
      'closeTab',
      'acceptTabOrganization',
      'rejectTabOrganization',
      'getProfileData',
      'getTabOrganizationSession',
      'openRecentlyClosedEntry',
      'requestTabOrganization',
      'switchToTab',
      'saveRecentlyClosedExpandedPref',
      'setTabIndex',
      'startTabGroupTutorial',
      'showUi',
    ]);

    this.callbackRouter = new PageCallbackRouter();

    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();
  }

  closeTab(tabId: number) {
    this.methodCalled('closeTab', [tabId]);
  }

  acceptTabOrganization(
      sessionId: number, organizationId: number, name: string, tabs: Tab[]) {
    this.methodCalled(
        'acceptTabOrganization', [sessionId, organizationId, name, tabs]);
  }

  rejectTabOrganization(sessionId: number, organizationId: number) {
    this.methodCalled('rejectTabOrganization', [sessionId, organizationId]);
  }

  getProfileData() {
    this.methodCalled('getProfileData');
    return Promise.resolve({profileData: this.profileData_!});
  }

  getTabOrganizationSession() {
    this.methodCalled('getTabOrganizationSession');
    return Promise.resolve({session: this.tabOrganizationSession_!});
  }

  openRecentlyClosedEntry(
      id: number, withSearch: boolean, isTab: boolean, index: number) {
    this.methodCalled(
        'openRecentlyClosedEntry', [id, withSearch, isTab, index]);
  }

  requestTabOrganization() {
    this.methodCalled('requestTabOrganization');
    return Promise.resolve({name: '', tabs: []});
  }

  switchToTab(info: SwitchToTabInfo) {
    this.methodCalled('switchToTab', [info]);
  }

  saveRecentlyClosedExpandedPref(expanded: boolean) {
    this.methodCalled('saveRecentlyClosedExpandedPref', [expanded]);
  }

  setTabIndex(index: number) {
    this.methodCalled('setTabIndex', [index]);
  }

  startTabGroupTutorial() {
    this.methodCalled('startTabGroupTutorial');
  }

  showUi() {
    this.methodCalled('showUi');
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

  setSession(session: TabOrganizationSession) {
    this.tabOrganizationSession_ = session;
  }
}
