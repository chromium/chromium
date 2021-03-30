// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PDFPlugin, PluginProxy} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {TestBrowserProxy} from '../test_browser_proxy.m.js';

/**
 * Test version of the PluginProxy.
 * @implements {PluginProxy}
 */
export class TestPluginProxy extends TestBrowserProxy {
  constructor() {
    super(['loadPreviewPage']);

    /** @type {?Function} The callback to call on load. */
    this.loadCompleteCallback_ = null;

    /** @type {?Function} Callback to call before load. */
    this.preloadCallback_ = null;

    /** @type {?Function} The callback to call when the viewport changes. */
    this.viewportChangedCallback_ = null;

    /** @type {boolean} Whether the plugin is compatible. */
    this.compatible_ = true;

    /** @type {?HTMLDivElement} */
    this.fakePlugin_ = null;
  }

  /**
   * @param {boolean} compatible Whether the PDF plugin should be compatible.
   */
  setPluginCompatible(compatible) {
    this.compatible_ = compatible;
  }

  /** @override */
  setLoadCompleteCallback(loadCompleteCallback) {
    assert(!this.loadCompleteCallback_);
    this.loadCompleteCallback_ = loadCompleteCallback;
  }

  /** @param {?Function} preloadCallback */
  setPreloadCallback(preloadCallback) {
    this.preloadCallback_ = preloadCallback;
  }

  /** @override */
  setKeyEventCallback(keyEventCallback) {}

  /** @override */
  setViewportChangedCallback(viewportChangedCallback) {
    this.viewportChangedCallback_ = viewportChangedCallback;
  }

  /** @override */
  checkPluginCompatibility(oopCompatObj) {
    return this.compatible_;
  }

  /** @override */
  pluginReady() {
    return !!this.fakePlugin_;
  }

  /** @override */
  createPlugin(previewUid, index) {
    this.fakePlugin_ =
        /** @type {!HTMLDivElement} */ (document.createElement('div'));
    this.fakePlugin_.classList.add('preview-area-plugin');
    this.fakePlugin_.id = 'pdf-viewer';
    return /** @type {!PDFPlugin} */ (this.fakePlugin_);
  }

  /** @override */
  resetPrintPreviewMode(previewUid, index, color, pages, modifiable) {}

  /** @override */
  scrollPosition(scrollX, scrollY) {}

  /** @override */
  sendKeyEvent(e) {}

  /** @override */
  hideToolbar() {}

  /** @override */
  setPointerEvents(eventsOn) {}

  /** @override */
  loadPreviewPage(previewUid, pageIndex, index) {
    this.methodCalled(
        'loadPreviewPage',
        {previewUid: previewUid, pageIndex: pageIndex, index: index});
    if (this.preloadCallback_) {
      this.preloadCallback_();
    }
    if (this.loadCompleteCallback_) {
      this.loadCompleteCallback_(true);
    }
  }

  /** @override */
  darkModeChanged(darkMode) {}

  /**
   * @param {number} pageX The horizontal offset for the page corner in
   *     pixels.
   * @param {number} pageY The vertical offset for the page corner in pixels.
   * @param {number} pageWidth The page width in pixels.
   * @param {number} viewportWidth The viewport width in pixels.
   * @param {number} viewportHeight The viewport height in pixels.
   */
  triggerVisualStateChange(
      pageX, pageY, pageWidth, viewportWidth, viewportHeight) {
    this.viewportChangedCallback_.apply(this, arguments);
  }
}
