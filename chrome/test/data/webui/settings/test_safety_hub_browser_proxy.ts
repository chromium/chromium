
// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {CardInfo, EntryPointInfo, NotificationPermission, SafetyHubBrowserProxy, UnusedSitePermissions} from 'chrome://settings/lazy_load.js';
import {CardState} from 'chrome://settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
// clang-format on

/**
 * A test version of SafetyHubBrowserProxy. Provides helper
 * methods for allowing tests to know when a method was called, as well as
 * specifying mock responses.
 */
export class TestSafetyHubBrowserProxy extends TestBrowserProxy implements
    SafetyHubBrowserProxy {
  private dummyCardInfo: CardInfo = {
    header: 'Dummy Header',
    subheader: 'Dummy Subheader',
    state: CardState.INFO,
  };

  private dummyEntryPointInfo: EntryPointInfo = {
    hasRecommendations: false,
    header: 'Dummy Header',
    subheader: 'Dummy Subheader',
  };

  private unusedSitePermissions_: UnusedSitePermissions[] = [];
  private reviewNotificationList_: NotificationPermission[] = [];
  private numberOfExtensionsThatNeedReview_: number = 0;
  private passwordCardData_: CardInfo = this.dummyCardInfo;
  private safeBrowsingCardData_: CardInfo = this.dummyCardInfo;
  private versionCardData_: CardInfo = this.dummyCardInfo;
  private entryPointData_: EntryPointInfo = this.dummyEntryPointInfo;

  constructor() {
    super([
      'acknowledgeRevokedUnusedSitePermissionsList',
      'allowPermissionsAgainForUnusedSite',
      'getRevokedUnusedSitePermissionsList',
      'getNumberOfExtensionsThatNeedReview',
      'undoAcknowledgeRevokedUnusedSitePermissionsList',
      'undoAllowPermissionsAgainForUnusedSite',
      'getNotificationPermissionReview',
      'blockNotificationPermissionForOrigins',
      'allowNotificationPermissionForOrigins',
      'ignoreNotificationPermissionForOrigins',
      'undoIgnoreNotificationPermissionForOrigins',
      'resetNotificationPermissionForOrigins',
      'getPasswordCardData',
      'getSafeBrowsingCardData',
      'getVersionCardData',
      'getSafetyHubEntryPointData',
      'dismissActiveMenuNotification',
      'recordSafetyHubPageVisit',
      'recordSafetyHubInteraction',
    ]);
  }

  acknowledgeRevokedUnusedSitePermissionsList() {
    this.methodCalled('acknowledgeRevokedUnusedSitePermissionsList');
  }

  allowPermissionsAgainForUnusedSite(origin: string) {
    this.methodCalled('allowPermissionsAgainForUnusedSite', [origin]);
  }

  setUnusedSitePermissions(unusedSitePermissionsList: UnusedSitePermissions[]) {
    this.unusedSitePermissions_ = unusedSitePermissionsList;
  }

  getRevokedUnusedSitePermissionsList(): Promise<UnusedSitePermissions[]> {
    this.methodCalled('getRevokedUnusedSitePermissionsList');
    return Promise.resolve(this.unusedSitePermissions_.slice());
  }

  undoAcknowledgeRevokedUnusedSitePermissionsList(unusedSitePermissionList:
                                                      UnusedSitePermissions[]) {
    this.methodCalled(
        'undoAcknowledgeRevokedUnusedSitePermissionsList',
        [unusedSitePermissionList]);
  }

  undoAllowPermissionsAgainForUnusedSite(unusedSitePermissions:
                                             UnusedSitePermissions) {
    this.methodCalled(
        'undoAllowPermissionsAgainForUnusedSite', [unusedSitePermissions]);
  }

  getNotificationPermissionReview(): Promise<NotificationPermission[]> {
    this.methodCalled('getNotificationPermissionReview');
    return Promise.resolve(this.reviewNotificationList_.slice());
  }

  setNotificationPermissionReview(reviewNotificationList:
                                      NotificationPermission[]) {
    this.reviewNotificationList_ = reviewNotificationList;
  }

  blockNotificationPermissionForOrigins(origins: string[]): void {
    this.methodCalled('blockNotificationPermissionForOrigins', origins);
  }

  allowNotificationPermissionForOrigins(origins: string[]): void {
    this.methodCalled('allowNotificationPermissionForOrigins', origins);
  }

  ignoreNotificationPermissionForOrigins(origins: string[]): void {
    this.methodCalled('ignoreNotificationPermissionForOrigins', origins);
  }

  undoIgnoreNotificationPermissionForOrigins(origins: string[]): void {
    this.methodCalled('undoIgnoreNotificationPermissionForOrigins', origins);
  }

  resetNotificationPermissionForOrigins(origins: string[]): void {
    this.methodCalled('resetNotificationPermissionForOrigins', origins);
  }

  setNumberOfExtensionsThatNeedReview(numberExtensions: number) {
    this.numberOfExtensionsThatNeedReview_ = numberExtensions;
  }

  getNumberOfExtensionsThatNeedReview(): Promise<number> {
    this.methodCalled('getNumberOfExtensionsThatNeedReview');
    return Promise.resolve(this.numberOfExtensionsThatNeedReview_);
  }

  getPasswordCardData(): Promise<CardInfo> {
    this.methodCalled('getPasswordCardData');
    return Promise.resolve(this.passwordCardData_);
  }

  setPasswordCardData(data: CardInfo): void {
    this.passwordCardData_ = data;
  }

  getSafeBrowsingCardData(): Promise<CardInfo> {
    this.methodCalled('getSafeBrowsingCardData');
    return Promise.resolve(this.safeBrowsingCardData_);
  }

  setSafeBrowsingCardData(data: CardInfo): void {
    this.safeBrowsingCardData_ = data;
  }

  getVersionCardData(): Promise<CardInfo> {
    this.methodCalled('getVersionCardData');
    return Promise.resolve(this.versionCardData_);
  }

  setVersionCardData(data: CardInfo): void {
    this.versionCardData_ = data;
  }

  getSafetyHubEntryPointData() {
    return Promise.resolve(this.entryPointData_);
  }

  setSafetyHubEntryPointData(value: EntryPointInfo) {
    this.entryPointData_ = value;
  }

  dismissActiveMenuNotification() {
    this.methodCalled('dismissActiveMenuNotification');
  }

  recordSafetyHubPageVisit() {
    this.methodCalled('recordSafetyHubPageVisit');
  }

  recordSafetyHubInteraction() {
    this.methodCalled('recordSafetyHubInteraction');
  }
}
