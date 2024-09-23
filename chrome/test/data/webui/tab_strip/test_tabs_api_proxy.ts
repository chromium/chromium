// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageRemote} from 'chrome://tab-strip.top-chrome/tab_strip.mojom-webui.js';
import {PageCallbackRouter} from 'chrome://tab-strip.top-chrome/tab_strip.mojom-webui.js';
import type {Tab, TabGroupVisualData} from 'chrome://tab-strip.top-chrome/tab_strip.mojom-webui.js';
import {TabNetworkState} from 'chrome://tab-strip.top-chrome/tab_strip.mojom-webui.js';
import type {CloseTabAction, TabsApiProxy} from 'chrome://tab-strip.top-chrome/tabs_api_proxy.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export function createTab(override?: Partial<Tab>): Tab {
  return Object.assign(
      {
        active: false,
        alertStates: [],
        blocked: false,
        crashed: false,
        faviconUrl: null,
        id: -1,
        index: -1,
        isDefaultFavicon: false,
        activeFaviconUrl: null,
        networkState: TabNetworkState.kNone,
        pinned: false,
        shouldHideThrobber: false,
        showIcon: false,
        groupId: null,
        title: '',
        url: {url: 'about:blank'},
      },
      override || {});
}

export class TestTabsApiProxy extends TestBrowserProxy implements TabsApiProxy {
  callbackRouter: PageCallbackRouter;
  callbackRouterRemote: PageRemote;
  private groupVisualData_: {[key: string]: TabGroupVisualData} = {};
  private tabs_: Tab[] = [];
  private thumbnailRequestCounts_: Map<number, number>;
  private layout_: {[key: string]: string} = {};
  private visible_: boolean = false;

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

    this.callbackRouter = new PageCallbackRouter();

    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();

    this.thumbnailRequestCounts_ = new Map();
  }

  getCallbackRouter() {
    return this.callbackRouter;
  }

  getCallbackRouterRemote(): PageRemote {
    return this.callbackRouterRemote;
  }

  activateTab(tabId: number) {
    this.methodCalled('activateTab', tabId);
    return Promise.resolve({
      active: true,
      autoDiscardable: false,
      discareded: false,
      groupId: 0,
      highlighted: false,
      id: tabId,
      incognito: false,
      index: 0,
      pinned: false,
      selected: false,
      windowId: 0,
    });
  }

  closeTab(tabId: number, closeTabAction: CloseTabAction) {
    this.methodCalled('closeTab', [tabId, closeTabAction]);
  }

  getGroupVisualData() {
    this.methodCalled('getGroupVisualData');
    return Promise.resolve({data: this.groupVisualData_});
  }

  getTabs() {
    this.methodCalled('getTabs');
    return Promise.resolve({tabs: this.tabs_.slice()});
  }

  getThumbnailRequestCount(tabId: number): number {
    return this.thumbnailRequestCounts_.get(tabId) || 0;
  }

  groupTab(tabId: number, groupId: string) {
    this.methodCalled('groupTab', [tabId, groupId]);
  }

  moveGroup(groupId: string, newIndex: number) {
    this.methodCalled('moveGroup', [groupId, newIndex]);
  }

  moveTab(tabId: number, newIndex: number) {
    this.methodCalled('moveTab', [tabId, newIndex]);
  }

  resetThumbnailRequestCounts() {
    this.thumbnailRequestCounts_.clear();
  }

  setGroupVisualData(data: {[key: string]: TabGroupVisualData}) {
    this.groupVisualData_ = data;
  }

  setTabs(tabs: Tab[]) {
    this.tabs_ = tabs;
  }

  setThumbnailTracked(tabId: number, thumbnailTracked: boolean) {
    if (thumbnailTracked) {
      this.thumbnailRequestCounts_.set(
          tabId, this.getThumbnailRequestCount(tabId) + 1);
    }
    this.methodCalled('setThumbnailTracked', [tabId, thumbnailTracked]);
  }

  ungroupTab(tabId: number) {
    this.methodCalled('ungroupTab', [tabId]);
  }

  getLayout() {
    this.methodCalled('getLayout');
    return Promise.resolve({layout: this.layout_});
  }

  isVisible() {
    this.methodCalled('isVisible');
    return this.visible_;
  }

  setLayout(layout: {[key: string]: string}) {
    this.layout_ = layout;
  }

  setVisible(visible: boolean) {
    this.visible_ = visible;
  }

  observeThemeChanges() {
    this.methodCalled('observeThemeChanges');
  }

  closeContainer() {
    this.methodCalled('closeContainer');
  }

  showBackgroundContextMenu(locationX: number, locationY: number) {
    this.methodCalled('showBackgroundContextMenu', [locationX, locationY]);
  }

  showEditDialogForGroup(
      groupId: string, locationX: number, locationY: number, width: number,
      height: number) {
    this.methodCalled(
        'showEditDialogForGroup',
        [groupId, locationX, locationY, width, height]);
  }

  showTabContextMenu(tabId: number, locationX: number, locationY: number) {
    this.methodCalled('showTabContextMenu', [tabId, locationX, locationY]);
  }

  reportTabActivationDuration(durationMs: number) {
    this.methodCalled('reportTabActivationDuration', [durationMs]);
  }

  reportTabDataReceivedDuration(tabCount: number, durationMs: number) {
    this.methodCalled('reportTabDataReceivedDuration', [tabCount, durationMs]);
  }

  reportTabCreationDuration(tabCount: number, durationMs: number) {
    this.methodCalled('reportTabCreationDuration', [tabCount, durationMs]);
  }
}
