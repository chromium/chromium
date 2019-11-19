// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeChromeEvent} from '../fake_chrome_event.m.js';
import {TestBrowserProxy} from '../test_browser_proxy.m.js';

/** An extensions.Service implementation to be used in tests. */
export class TestService extends TestBrowserProxy {
  constructor() {
    super([
      'addRuntimeHostPermission',
      'deleteActivitiesById',
      'deleteActivitiesFromExtension',
      'downloadActivities',
      'getExtensionActivityLog',
      'getExtensionsInfo',
      'getExtensionSize',
      'getFilteredExtensionActivityLog',
      'getProfileConfiguration',
      'loadUnpacked',
      'retryLoadUnpacked',
      'reloadItem',
      'removeRuntimeHostPermission',
      'setItemHostAccess',
      'setProfileInDevMode',
      'setShortcutHandlingSuspended',
      'shouldIgnoreUpdate',
      'updateAllExtensions',
      'updateExtensionCommandKeybinding',
      'updateExtensionCommandScope',
    ]);

    this.itemStateChangedTarget = new FakeChromeEvent();
    this.profileStateChangedTarget = new FakeChromeEvent();
    this.extensionActivityTarget = new FakeChromeEvent();

    /** @type {boolean} */
    this.acceptRuntimeHostPermission = true;

    /** @private {!chrome.developerPrivate.LoadError} */
    this.retryLoadUnpackedError_;

    /** @type {boolean} */
    this.forceReloadItemError_ = false;

    /** @type {!chrome.activityLogPrivate.ActivityResultSet|undefined} */
    this.testActivities;
  }

  /**
   * @param {!chrome.developerPrivate.LoadError} error
   */
  setRetryLoadUnpackedError(error) {
    this.retryLoadUnpackedError_ = error;
  }

  /**
   * @param {boolean} force
   */
  setForceReloadItemError(force) {
    this.forceReloadItemError_ = force;
  }

  /** @override */
  addRuntimeHostPermission(id, site) {
    this.methodCalled('addRuntimeHostPermission', [id, site]);
    return this.acceptRuntimeHostPermission ? Promise.resolve() :
                                              Promise.reject();
  }

  /** @override */
  getProfileConfiguration() {
    this.methodCalled('getProfileConfiguration');
    return Promise.resolve({inDeveloperMode: false});
  }

  /** @override */
  getItemStateChangedTarget() {
    return this.itemStateChangedTarget;
  }

  /** @override */
  getProfileStateChangedTarget() {
    return this.profileStateChangedTarget;
  }

  /** @override */
  getExtensionsInfo() {
    this.methodCalled('getExtensionsInfo');
    return Promise.resolve([]);
  }

  /** @override */
  getExtensionSize() {
    this.methodCalled('getExtensionSize');
    return Promise.resolve('20 MB');
  }

  /** @override */
  removeRuntimeHostPermission(id, site) {
    this.methodCalled('removeRuntimeHostPermission', [id, site]);
    return Promise.resolve();
  }

  /** @override */
  setItemHostAccess(id, access) {
    this.methodCalled('setItemHostAccess', [id, access]);
  }

  /** @override */
  setShortcutHandlingSuspended(enable) {
    this.methodCalled('setShortcutHandlingSuspended', enable);
  }

  /** @override */
  shouldIgnoreUpdate(extensionId, eventType) {
    this.methodCalled('shouldIgnoreUpdate', [extensionId, eventType]);
  }

  /** @override */
  updateExtensionCommandKeybinding(item, commandName, keybinding) {
    this.methodCalled(
        'updateExtensionCommandKeybinding', [item, commandName, keybinding]);
  }

  /** @override */
  updateExtensionCommandScope(item, commandName, scope) {
    this.methodCalled(
        'updateExtensionCommandScope', [item, commandName, scope]);
  }

  /** @override */
  loadUnpacked() {
    this.methodCalled('loadUnpacked');
    return Promise.resolve();
  }

  /** @override */
  reloadItem(id) {
    this.methodCalled('reloadItem', id);
    return this.forceReloadItemError_ ? Promise.reject() : Promise.resolve();
  }

  /** @override */
  retryLoadUnpacked(guid) {
    this.methodCalled('retryLoadUnpacked', guid);
    return (this.retryLoadUnpackedError_ !== undefined) ?
        Promise.reject(this.retryLoadUnpackedError_) :
        Promise.resolve();
  }

  /** @override */
  setProfileInDevMode(inDevMode) {
    this.methodCalled('setProfileInDevMode', inDevMode);
  }

  /** @override */
  updateAllExtensions() {
    this.methodCalled('updateAllExtensions');
    return Promise.resolve();
  }

  /** @override */
  getExtensionActivityLog(id) {
    this.methodCalled('getExtensionActivityLog', id);
    return Promise.resolve(this.testActivities);
  }

  /** @override */
  getFilteredExtensionActivityLog(id, searchTerm) {
    // This is functionally identical to getFilteredExtensionActivityLog in
    // service.js but we do the filtering here instead of making API calls
    // with filter objects.
    this.methodCalled('getFilteredExtensionActivityLog', id, searchTerm);

    // Convert everything to lowercase as searching is not case sensitive.
    const lowerCaseSearchTerm = searchTerm.toLowerCase();

    const activities = this.testActivities.activities;
    const apiCallMatches = activities.filter(
        activity =>
            activity.apiCall.toLowerCase().includes(lowerCaseSearchTerm));
    const pageUrlMatches = activities.filter(
        activity => activity.pageUrl &&
            activity.pageUrl.toLowerCase().includes(lowerCaseSearchTerm));
    const argUrlMatches = activities.filter(
        activity => activity.argUrl &&
            activity.argUrl.toLowerCase().includes(lowerCaseSearchTerm));

    return Promise.resolve(
        {activities: [...apiCallMatches, ...pageUrlMatches, ...argUrlMatches]});
  }

  /** @override */
  deleteActivitiesById(activityIds) {
    // Pretend to delete all activities specified by activityIds.
    const newActivities = this.testActivities.activities.filter(
        activity => !activityIds.includes(activity.activityId));
    this.testActivities = {activities: newActivities};

    this.methodCalled('deleteActivitiesById', activityIds);
    return Promise.resolve();
  }

  /** @override */
  deleteActivitiesFromExtension(extensionId) {
    this.methodCalled('deleteActivitiesFromExtension', extensionId);
    return Promise.resolve();
  }

  /** @override */
  getOnExtensionActivity() {
    return this.extensionActivityTarget;
  }

  /** @override */
  downloadActivities(rawActivityData, fileName) {
    this.methodCalled('downloadActivities', [rawActivityData, fileName]);
  }
}
