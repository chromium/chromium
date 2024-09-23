// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageRemote, ProfileData, SwitchToTabInfo, Tab, TabOrganizationSession, TabSearchApiProxy, UserFeedback} from 'chrome://tab-search.top-chrome/tab_search.js';
import {PageCallbackRouter, TabOrganizationFeature, TabOrganizationModelStrategy} from 'chrome://tab-search.top-chrome/tab_search.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestTabSearchApiProxy extends TestBrowserProxy implements
    TabSearchApiProxy {
  callbackRouter: PageCallbackRouter;
  callbackRouterRemote: PageRemote;
  private profileData_?: ProfileData;
  private tabOrganizationSession_?: TabOrganizationSession;
  private staleTabs_: Tab[] = [];

  constructor() {
    super([
      'closeTab',
      'declutterTabs',
      'acceptTabOrganization',
      'rejectTabOrganization',
      'renameTabOrganization',
      'excludeFromStaleTabs',
      'getProfileData',
      'getStaleTabs',
      'getTabOrganizationFeature',
      'getTabOrganizationSession',
      'getTabOrganizationModelStrategy',
      'openRecentlyClosedEntry',
      'requestTabOrganization',
      'removeTabFromOrganization',
      'rejectSession',
      'restartSession',
      'switchToTab',
      'saveRecentlyClosedExpandedPref',
      'setTabIndex',
      'setOrganizationFeature',
      'startTabGroupTutorial',
      'triggerFeedback',
      'triggerSignIn',
      'openHelpPage',
      'setTabOrganizationModelStrategy',
      'setUserFeedback',
      'notifyOrganizationUiReadyToShow',
      'notifySearchUiReadyToShow',
    ]);

    this.callbackRouter = new PageCallbackRouter();

    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();
  }

  closeTab(tabId: number) {
    this.methodCalled('closeTab', [tabId]);
  }

  declutterTabs(tabIds: number[]) {
    this.methodCalled('declutterTabs', [tabIds]);
  }

  acceptTabOrganization(
      sessionId: number, organizationId: number, tabs: Tab[]) {
    this.methodCalled(
        'acceptTabOrganization', [sessionId, organizationId, tabs]);
  }

  rejectTabOrganization(sessionId: number, organizationId: number) {
    this.methodCalled('rejectTabOrganization', [sessionId, organizationId]);
  }

  renameTabOrganization(
      sessionId: number, organizationId: number, name: string) {
    this.methodCalled(
        'renameTabOrganization', [sessionId, organizationId, name]);
  }

  excludeFromStaleTabs(tabId: number) {
    this.methodCalled('excludeFromStaleTabs', [tabId]);
  }

  getProfileData() {
    this.methodCalled('getProfileData');
    return Promise.resolve({profileData: this.profileData_!});
  }

  getStaleTabs() {
    this.methodCalled('getStaleTabs');
    return Promise.resolve({tabs: this.staleTabs_});
  }

  getTabOrganizationFeature() {
    this.methodCalled('getTabOrganizationFeature');
    return Promise.resolve({feature: TabOrganizationFeature.kSelector});
  }

  getTabOrganizationSession() {
    this.methodCalled('getTabOrganizationSession');
    return Promise.resolve({session: this.tabOrganizationSession_!});
  }

  getTabOrganizationModelStrategy() {
    this.methodCalled('getTabOrganizationModelStrategy');
    return Promise.resolve({strategy: TabOrganizationModelStrategy.kTopic});
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

  removeTabFromOrganization(
      sessionId: number, organizationId: number, tab: Tab) {
    this.methodCalled(
        'removeTabFromOrganization', sessionId, organizationId, tab);
  }

  rejectSession() {
    this.methodCalled('rejectSession');
  }

  restartSession() {
    this.methodCalled('restartSession');
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

  setOrganizationFeature(feature: TabOrganizationFeature) {
    this.methodCalled('setOrganizationFeature', [feature]);
  }

  startTabGroupTutorial() {
    this.methodCalled('startTabGroupTutorial');
  }

  triggerFeedback(sessionId: number) {
    this.methodCalled('triggerFeedback', [sessionId]);
  }

  triggerSignIn() {
    this.methodCalled('triggerSignIn');
  }

  openHelpPage() {
    this.methodCalled('openHelpPage');
  }

  setTabOrganizationModelStrategy(strategy: TabOrganizationModelStrategy) {
    this.methodCalled('setTabOrganizationModelStrategy', [strategy]);
  }

  setUserFeedback(feedback: UserFeedback) {
    this.methodCalled('setUserFeedback', [feedback]);
  }

  notifyOrganizationUiReadyToShow() {
    this.methodCalled('notifyOrganizationUiReadyToShow');
  }

  notifySearchUiReadyToShow() {
    this.methodCalled('notifySearchUiReadyToShow');
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

  setStaleTabs(tabs: Tab[]) {
    this.staleTabs_ = tabs;
  }
}
