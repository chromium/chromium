// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageRemote} from 'chrome://tab-strip.top-chrome/tab_strip.mojom-webui.js';
import {Tab, TabGroupVisualData} from 'chrome://tab-strip.top-chrome/tab_strip.mojom-webui.js';
import {ExtensionsApiTab, TabsApiProxy} from 'chrome://tab-strip.top-chrome/tabs_api_proxy.js';

import {TestBrowserProxy} from '../test_browser_proxy.js';

/** @implements {TabsApiProxy} */
export class TestTabsApiProxy extends TestBrowserProxy {
  constructor() {
    super([
      'activateTab',
      'closeTab',
      'getGroupVisualData',
      'getTabs',
      'groupTab',
      'moveGroup',
      'moveTab',
      'setThumbnailTracked',
      'ungroupTab',
      'closeContainer',
      'getColors',
      'getLayout',
      'isVisible',
      'observeThemeChanges',
      'showBackgroundContextMenu',
      'showEditDialogForGroup',
      'showTabContextMenu',
      'reportTabActivationDuration',
      'reportTabDataReceivedDuration',
      'reportTabCreationDuration',
    ]);

    /** @type {!PageCallbackRouter} */
    this.callbackRouter = new PageCallbackRouter();

    /** @type {!PageRemote} */
    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();

    /** @type {!Object<!TabGroupVisualData>} */
    this.groupVisualData_;

    /** @type {!Array<!Tab>} */
    this.tabs_;

    /** @type {!Map<number, number>} */
    this.thumbnailRequestCounts_ = new Map();

    /** @private {!Object<string, string>} */
    this.colors_ = {};

    /** @private {!Object<string, string>} */
    this.layout_ = {};

    /** @private {boolean} */
    this.visible_ = false;
  }

  /** @override */
  getCallbackRouter() {
    return this.callbackRouter;
  }

  /** return {!PageRemote} */
  getCallbackRouterRemote() {
    return this.callbackRouterRemote;
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
  getGroupVisualData() {
    this.methodCalled('getGroupVisualData');
    return Promise.resolve({data: this.groupVisualData_});
  }

  /** @override */
  getTabs() {
    this.methodCalled('getTabs');
    return Promise.resolve({tabs: this.tabs_.slice()});
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

  /** @param {!Object<!TabGroupVisualData>} data */
  setGroupVisualData(data) {
    this.groupVisualData_ = data;
  }

  /** @param {!Array<!Tab>} tabs */
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

  /** @override */
  getColors() {
    this.methodCalled('getColors');
    return Promise.resolve({colors: this.colors_});
  }

  /** @override */
  getLayout() {
    this.methodCalled('getLayout');
    return Promise.resolve({layout: this.layout_});
  }

  /** @override */
  isVisible() {
    this.methodCalled('isVisible');
    return this.visible_;
  }

  /** @param {!Object<string, string>} colors */
  setColors(colors) {
    this.colors_ = colors;
  }

  /** @param {!Object<string, string>} layout */
  setLayout(layout) {
    this.layout_ = layout;
  }

  /** @param {boolean} visible */
  setVisible(visible) {
    this.visible_ = visible;
  }

  /** @override */
  observeThemeChanges() {
    this.methodCalled('observeThemeChanges');
  }

  /** @override */
  closeContainer() {
    this.methodCalled('closeContainer');
  }

  /** @override */
  showBackgroundContextMenu(locationX, locationY) {
    this.methodCalled('showBackgroundContextMenu', [locationX, locationY]);
  }

  /** @override */
  showEditDialogForGroup(groupId, locationX, locationY, width, height) {
    this.methodCalled(
        'showEditDialogForGroup',
        [groupId, locationX, locationY, width, height]);
  }

  /** @override */
  showTabContextMenu(tabId, locationX, locationY) {
    this.methodCalled('showTabContextMenu', [tabId, locationX, locationY]);
  }

  /** @override */
  reportTabActivationDuration(durationMs) {
    this.methodCalled('reportTabActivationDuration', [durationMs]);
  }

  /** @override */
  reportTabDataReceivedDuration(tabCount, durationMs) {
    this.methodCalled('reportTabDataReceivedDuration', [tabCount, durationMs]);
  }

  /** @override */
  reportTabCreationDuration(tabCount, durationMs) {
    this.methodCalled('reportTabCreationDuration', [tabCount, durationMs]);
  }
}
