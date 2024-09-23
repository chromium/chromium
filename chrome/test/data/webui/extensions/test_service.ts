// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ServiceInterface} from 'chrome://extensions/extensions.js';
import {FakeChromeEvent} from 'chrome://webui-test/fake_chrome_event.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

// An Service implementation to be used in tests.
export class TestService extends TestBrowserProxy implements ServiceInterface {
  itemStateChangedTarget: FakeChromeEvent = new FakeChromeEvent();
  profileStateChangedTarget: FakeChromeEvent = new FakeChromeEvent();
  extensionActivityTarget: FakeChromeEvent = new FakeChromeEvent();
  userSiteSettingsChangedTarget: FakeChromeEvent = new FakeChromeEvent();
  acceptRuntimeHostPermission: boolean = true;
  testActivities?: chrome.activityLogPrivate.ActivityResultSet;
  userSiteSettings?: chrome.developerPrivate.UserSiteSettings;
  siteGroups?: chrome.developerPrivate.SiteGroup[];
  matchingExtensionsInfo?: chrome.developerPrivate.MatchingExtensionInfo[];

  private retryLoadUnpackedError_?: chrome.developerPrivate.LoadError;
  private forceReloadItemError_: boolean = false;
  private loadUnpackedSuccess_: boolean = true;

  constructor() {
    super([
      'addRuntimeHostPermission',
      'addUserSpecifiedSites',
      'choosePackRootDirectory',
      'choosePrivateKeyPath',
      'deleteActivitiesById',
      'deleteActivitiesFromExtension',
      'deleteErrors',
      'deleteItem',
      'deleteItems',
      'dismissSafetyHubExtensionsMenuNotification',
      'dismissMv2DeprecationNotice',
      'dismissMv2DeprecationNoticeForExtension',
      'uninstallItem',
      'downloadActivities',
      'getExtensionActivityLog',
      'getExtensionsInfo',
      'getExtensionSize',
      'getFilteredExtensionActivityLog',
      'getMatchingExtensionsForSite',
      'getProfileConfiguration',
      'getUserAndExtensionSitesByEtld',
      'getUserSiteSettings',
      'getUserSiteSettingsChangedTarget',
      'inspectItemView',
      'installDroppedFile',
      'loadUnpacked',
      'loadUnpackedFromDrag',
      'notifyDragInstallInProgress',
      'openUrl',
      'packExtension',
      'recordUserAction',
      'reloadItem',
      'removeRuntimeHostPermission',
      'removeUserSpecifiedSites',
      'repairItem',
      'requestFileSource',
      'retryLoadUnpacked',
      'setItemAllowedIncognito',
      'setItemAllowedOnFileUrls',
      'setItemCollectsErrors',
      'setItemEnabled',
      'setItemHostAccess',
      'setItemPinnedToToolbar',
      'setItemSafetyCheckWarningAcknowledged',
      'setProfileInDevMode',
      'setShortcutHandlingSuspended',
      'setShowAccessRequestsInToolbar',
      'shouldIgnoreUpdate',
      'showInFolder',
      'showItemOptionsPage',
      'updateAllExtensions',
      'updateExtensionCommandKeybinding',
      'updateExtensionCommandScope',
      'updateSiteAccess',
    ]);
  }

  setRetryLoadUnpackedError(error: chrome.developerPrivate.LoadError) {
    this.retryLoadUnpackedError_ = error;
  }

  setForceReloadItemError(force: boolean) {
    this.forceReloadItemError_ = force;
  }

  setLoadUnpackedSuccess(success: boolean) {
    this.loadUnpackedSuccess_ = success;
  }

  addRuntimeHostPermission(id: string, host: string) {
    this.methodCalled('addRuntimeHostPermission', [id, host]);
    return this.acceptRuntimeHostPermission ? Promise.resolve() :
                                              Promise.reject();
  }

  choosePackRootDirectory() {
    this.methodCalled('choosePackRootDirectory');
    return Promise.resolve('');
  }

  choosePrivateKeyPath() {
    this.methodCalled('choosePrivateKeyPath');
    return Promise.resolve('');
  }

  getProfileConfiguration() {
    this.methodCalled('getProfileConfiguration');
    return Promise.resolve({
      canLoadUnpacked: false,
      inDeveloperMode: false,
      isDeveloperModeControlledByPolicy: false,
      isIncognitoAvailable: false,
      isChildAccount: false,
      isMv2DeprecationNoticeDismissed: false,
    });
  }

  getItemStateChangedTarget() {
    return this.itemStateChangedTarget;
  }

  getProfileStateChangedTarget() {
    return this.profileStateChangedTarget;
  }

  getUserSiteSettingsChangedTarget() {
    return this.userSiteSettingsChangedTarget;
  }

  getExtensionsInfo() {
    this.methodCalled('getExtensionsInfo');
    return Promise.resolve([]);
  }

  getExtensionSize() {
    this.methodCalled('getExtensionSize');
    return Promise.resolve('20 MB');
  }

  inspectItemView(id: string, view: chrome.developerPrivate.ExtensionView) {
    this.methodCalled('inspectItemView', [id, view]);
  }

  removeRuntimeHostPermission(id: string, host: string) {
    this.methodCalled('removeRuntimeHostPermission', [id, host]);
    return Promise.resolve();
  }

  setItemAllowedIncognito(id: string, isAllowedIncognito: boolean) {
    this.methodCalled('setItemAllowedIncognito', [id, isAllowedIncognito]);
  }

  setItemAllowedOnFileUrls(id: string, isAllowedOnFileUrls: boolean) {
    this.methodCalled('setItemAllowedOnFileUrls', [id, isAllowedOnFileUrls]);
  }

  setItemSafetyCheckWarningAcknowledged(id: string) {
    this.methodCalled('setItemSafetyCheckWarningAcknowledged', id);
  }

  setItemEnabled(id: string, isEnabled: boolean) {
    this.methodCalled('setItemEnabled', [id, isEnabled]);
  }

  setItemCollectsErrors(id: string, collectsErrors: boolean) {
    this.methodCalled('setItemCollectsErrors', [id, collectsErrors]);
  }

  setItemHostAccess(id: string, access: chrome.developerPrivate.HostAccess) {
    this.methodCalled('setItemHostAccess', [id, access]);
  }

  setItemPinnedToToolbar(id: string, pinnedToToolbar: boolean) {
    this.methodCalled('setItemPinnedToToolbar', [id, pinnedToToolbar]);
  }

  setShortcutHandlingSuspended(enable: boolean) {
    this.methodCalled('setShortcutHandlingSuspended', enable);
  }

  shouldIgnoreUpdate(
      extensionId: string, eventType: chrome.developerPrivate.EventType) {
    this.methodCalled('shouldIgnoreUpdate', [extensionId, eventType]);
  }

  updateExtensionCommandKeybinding(
      extensionId: string, commandName: string, keybinding: string) {
    this.methodCalled(
        'updateExtensionCommandKeybinding',
        [extensionId, commandName, keybinding]);
  }

  updateExtensionCommandScope(
      extensionId: string, commandName: string,
      scope: chrome.developerPrivate.CommandScope): void {
    this.methodCalled(
        'updateExtensionCommandScope', [extensionId, commandName, scope]);
  }

  loadUnpacked() {
    this.methodCalled('loadUnpacked');
    return Promise.resolve(this.loadUnpackedSuccess_);
  }

  reloadItem(id: string) {
    this.methodCalled('reloadItem', id);
    return this.forceReloadItemError_ ? Promise.reject() : Promise.resolve();
  }

  retryLoadUnpacked(guid: string) {
    this.methodCalled('retryLoadUnpacked', guid);
    return (this.retryLoadUnpackedError_ !== undefined) ?
        Promise.reject(this.retryLoadUnpackedError_) :
        Promise.resolve(true);
  }

  requestFileSource(args: chrome.developerPrivate.RequestFileSourceProperties) {
    this.methodCalled('requestFileSource', args);
    return Promise.resolve({
      highlight: '',
      beforeHighlight: '',
      afterHighlight: '',
      title: '',
      message: '',
    });
  }

  openUrl(url: string) {
    this.methodCalled('openUrl', url);
  }

  packExtension(rootPath: string, keyPath: string, flag?: number) {
    this.methodCalled('packExtension', [rootPath, keyPath, flag]);
    return Promise.resolve({
      message: '',
      item_path: '',
      pem_path: '',
      override_flags: 0,
      status: chrome.developerPrivate.PackStatus.ERROR,
    });
  }

  repairItem(id: string): void {
    this.methodCalled('repairItem', id);
  }

  setProfileInDevMode(inDevMode: boolean) {
    this.methodCalled('setProfileInDevMode', inDevMode);
  }

  showInFolder(id: string) {
    this.methodCalled('showInFolder', id);
  }

  showItemOptionsPage(extension: chrome.developerPrivate.ExtensionInfo) {
    this.methodCalled('showItemOptionsPage', extension);
  }

  updateAllExtensions(_extensions: chrome.developerPrivate.ExtensionInfo[]) {
    this.methodCalled('updateAllExtensions');
    return this.forceReloadItemError_ ? Promise.reject() : Promise.resolve();
  }

  getExtensionActivityLog(id: string) {
    this.methodCalled('getExtensionActivityLog', id);
    return Promise.resolve(this.testActivities!);
  }

  getFilteredExtensionActivityLog(id: string, searchTerm: string) {
    // This is functionally identical to getFilteredExtensionActivityLog in
    // service.js but we do the filtering here instead of making API calls
    // with filter objects.
    this.methodCalled('getFilteredExtensionActivityLog', id, searchTerm);

    // Convert everything to lowercase as searching is not case sensitive.
    const lowerCaseSearchTerm = searchTerm.toLowerCase();

    const activities = this.testActivities!.activities;
    const apiCallMatches = activities.filter(
        activity =>
            activity.apiCall!.toLowerCase().includes(lowerCaseSearchTerm));
    const pageUrlMatches = activities.filter(
        activity => activity.pageUrl &&
            activity.pageUrl.toLowerCase().includes(lowerCaseSearchTerm));
    const argUrlMatches = activities.filter(
        activity => activity.argUrl &&
            activity.argUrl.toLowerCase().includes(lowerCaseSearchTerm));

    return Promise.resolve(
        {activities: [...apiCallMatches, ...pageUrlMatches, ...argUrlMatches]});
  }

  deleteActivitiesById(activityIds: string[]) {
    // Pretend to delete all activities specified by activityIds.
    const newActivities = this.testActivities!.activities.filter(
        activity => !activityIds.includes(activity.activityId!));
    this.testActivities = {activities: newActivities};

    this.methodCalled('deleteActivitiesById', activityIds);
    return Promise.resolve();
  }

  deleteActivitiesFromExtension(extensionId: string) {
    this.methodCalled('deleteActivitiesFromExtension', extensionId);
    return Promise.resolve();
  }

  deleteErrors(
      extensionId: string, _errorIds?: number[],
      _type?: chrome.developerPrivate.ErrorType) {
    this.methodCalled('deleteErrors', extensionId);
  }

  deleteItem(id: string) {
    this.methodCalled('deleteItem', id);
  }

  deleteItems(ids: string[]) {
    this.methodCalled('deleteItems', ids);
    return Promise.resolve();
  }

  uninstallItem(id: string) {
    this.methodCalled('uninstallItem', id);
    return Promise.resolve();
  }

  getOnExtensionActivity() {
    return this.extensionActivityTarget;
  }

  downloadActivities(rawActivityData: string, fileName: string) {
    this.methodCalled('downloadActivities', [rawActivityData, fileName]);
  }

  recordUserAction(metricName: string) {
    this.methodCalled('recordUserAction', metricName);
  }

  notifyDragInstallInProgress() {
    this.methodCalled('notifyDragInstallInProgress');
  }

  loadUnpackedFromDrag() {
    this.methodCalled('loadUnpackedFromDrag');
    return Promise.resolve(true);
  }

  installDroppedFile() {
    this.methodCalled('installDroppedFile');
  }

  getUserSiteSettings() {
    this.methodCalled('getUserSiteSettings');
    return Promise.resolve(this.userSiteSettings!);
  }

  addUserSpecifiedSites(
      siteSet: chrome.developerPrivate.SiteSet, hosts: string[]) {
    this.methodCalled('addUserSpecifiedSites', [siteSet, hosts]);
    return Promise.resolve();
  }

  removeUserSpecifiedSites(
      siteSet: chrome.developerPrivate.SiteSet, hosts: string[]) {
    this.methodCalled('removeUserSpecifiedSites', [siteSet, hosts]);
    return Promise.resolve();
  }

  getUserAndExtensionSitesByEtld() {
    this.methodCalled('getUserAndExtensionSitesByEtld');
    return Promise.resolve(this.siteGroups!);
  }

  setShowAccessRequestsInToolbar(id: string, showRequests: boolean) {
    this.methodCalled('setShowAccessRequestsInToolbar', id, showRequests);
  }

  getMatchingExtensionsForSite(site: string) {
    this.methodCalled('getMatchingExtensionsForSite', site);
    return Promise.resolve(this.matchingExtensionsInfo!);
  }

  updateSiteAccess(
      site: string,
      updates: chrome.developerPrivate.ExtensionSiteAccessUpdate[]) {
    this.methodCalled('updateSiteAccess', site, updates);
    return Promise.resolve();
  }

  dismissSafetyHubExtensionsMenuNotification() {
    this.methodCalled('dismissSafetyHubExtensionsMenuNotification');
  }

  dismissMv2DeprecationNoticeForExtension(id: string) {
    this.methodCalled('dismissMv2DeprecationNoticeForExtension', id);
  }

  dismissMv2DeprecationNotice() {
    this.methodCalled('dismissMv2DeprecationNotice');
  }
}
