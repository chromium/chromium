// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TabStripEmbedderProxy} from 'chrome://tab-strip.top-chrome/tab_strip_embedder_proxy.js';

import {TestBrowserProxy} from '../test_browser_proxy.js';

/** @implements {TabStripEmbedderProxy} */
export class TestTabStripEmbedderProxy extends TestBrowserProxy {
  constructor() {
    super([
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

    /** @private {!Object<string, string>} */
    this.colors_ = {};

    /** @private {!Object<string, string>} */
    this.layout_ = {};

    /** @private {boolean} */
    this.visible_ = false;
  }

  /** @override */
  getColors() {
    this.methodCalled('getColors');
    return Promise.resolve(this.colors_);
  }

  /** @override */
  getLayout() {
    this.methodCalled('getLayout');
    return Promise.resolve(this.layout_);
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
