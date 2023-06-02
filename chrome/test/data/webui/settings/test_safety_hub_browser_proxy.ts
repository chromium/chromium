
// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {NotificationPermission, UnusedSitePermissions, SafetyHubBrowserProxy} from 'chrome://settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
// clang-format on

/**
 * A test version of SafetyHubBrowserProxy. Provides helper
 * methods for allowing tests to know when a method was called, as well as
 * specifying mock responses.
 */
export class TestSafetyHubBrowserProxy extends TestBrowserProxy implements
    SafetyHubBrowserProxy {
  private unusedSitePermissions_: UnusedSitePermissions[] = [];
  private reviewNotificationList_: NotificationPermission[] = [];

  constructor() {
    super([
      'acknowledgeRevokedUnusedSitePermissionsList',
      'allowPermissionsAgainForUnusedSite',
      'getRevokedUnusedSitePermissionsList',
      'undoAcknowledgeRevokedUnusedSitePermissionsList',
      'undoAllowPermissionsAgainForUnusedSite',
      'getNotificationPermissionReview',
      'blockNotificationPermissionForOrigins',
      'allowNotificationPermissionForOrigins',
      'ignoreNotificationPermissionForOrigins',
      'undoIgnoreNotificationPermissionForOrigins',
      'resetNotificationPermissionForOrigins',
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
}
