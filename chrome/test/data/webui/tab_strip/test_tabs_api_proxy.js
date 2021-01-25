// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ExtensionsApiTab, TabData, TabGroupVisualData, TabsApiProxy} from 'chrome://tab-strip/tabs_api_proxy.js';

import {TestBrowserProxy} from '../test_browser_proxy.m.js';

/** @implements {TabsApiProxy} */
export class TestTabsApiProxy extends TestBrowserProxy {
  constructor() {
    super([
      'activateTab',
      'closeTab',
      'createNewTab',
      'getGroupVisualData',
      'getTabs',
      'groupTab',
      'moveGroup',
      'moveTab',
      'setThumbnailTracked',
      'ungroupTab',
    ]);

    /** @type {!Object<!TabGroupVisualData>} */
    this.groupVisualData_;

    /** @type {!Array<!TabData>} */
    this.tabs_;

    /** @type {!Map<number, number>} */
    this.thumbnailRequestCounts_ = new Map();
  }

  /** @override */
  activateTab(tabId) {
    this.methodCalled('activateTab', tabId);
    return Promise.resolve(
        /** @type {!ExtensionsApiTab} */ ({active: true, id: tabId}));
  }

  /** @override */
  closeTab(tabId, closeTabAction) {
    this.methodCalled('closeTab', [tabId, closeTabAction]);
  }

  /** @override */
  createNewTab() {
    this.methodCalled('createNewTab');
  }

  /** @override */
  getGroupVisualData() {
    this.methodCalled('getGroupVisualData');
    return Promise.resolve(this.groupVisualData_);
  }

  /** @override */
  getTabs() {
    this.methodCalled('getTabs');
    return Promise.resolve(this.tabs_.slice());
  }

  /**
   * @param {number} tabId
   * @return {number}
   */
  getThumbnailRequestCount(tabId) {
    return this.thumbnailRequestCounts_.get(tabId) || 0;
  }

  /** @override */
  groupTab(tabId, groupId) {
    this.methodCalled('groupTab', [tabId, groupId]);
  }

  /** @override */
  moveGroup(groupId, newIndex) {
    this.methodCalled('moveGroup', [groupId, newIndex]);
  }

  /** @override */
  moveTab(tabId, newIndex) {
    this.methodCalled('moveTab', [tabId, newIndex]);
  }

  resetThumbnailRequestCounts() {
    this.thumbnailRequestCounts_.clear();
  }

  /** @param {!Object<!TabGroupVisualData>} groupVisualData */
  setGroupVisualData(groupVisualData) {
    this.groupVisualData_ = groupVisualData;
  }

  /** @param {!Array<!TabData>} tabs */
  setTabs(tabs) {
    this.tabs_ = tabs;
  }

  /** @override */
  setThumbnailTracked(tabId, thumbnailTracked) {
    if (thumbnailTracked) {
      this.thumbnailRequestCounts_.set(
          tabId, this.getThumbnailRequestCount(tabId) + 1);
    }
    this.methodCalled('setThumbnailTracked', [tabId, thumbnailTracked]);
  }

  /** @override */
  ungroupTab(tabId) {
    this.methodCalled('ungroupTab', [tabId]);
  }
}
