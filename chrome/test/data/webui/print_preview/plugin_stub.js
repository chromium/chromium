// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';

/**
 * Test version of the PluginProxy.
 */
export class PDFPluginStub extends TestBrowserProxy {
  constructor() {
    super(['loadPreviewPage']);

    /** @type {?Function} The callback to call on load. */
    this.loadCallback_ = null;

    /** @type {?Function} Callback to call before load. */
    this.preloadCallback_ = null;

    /** @type {?Function} The callback to call when the viewport changes. */
    this.viewportChangedCallback_ = null;

    /** @type {boolean} Whether the plugin is compatible. */
    this.compatible_ = true;

    /** @type {?HTMLDivElement} */
    this.fakePlugin_ = null;
  }

  /** @param {boolean} Whether the PDF plugin should be compatible. */
  setPluginCompatible(compatible) {
    this.compatible_ = compatible;
  }

  /**
   * @param {?Function} loadCallback Callback to call when the preview
   *     loads.
   */
  setLoadCallback(loadCallback) {
    assert(!this.loadCallback_);
    this.loadCallback_ = loadCallback;
  }

  /**
   * @param {?Function} preloadCallback Callback to call before the preview
   *     loads.
   */
  setPreloadCallback(preloadCallback) {
    this.preloadCallback_ = preloadCallback;
  }

  /** @param {?Function} keyEventCallback */
  setKeyEventCallback(keyEventCallback) {}

  /** @param {?Function} viewportChangedCallback */
  setViewportChangedCallback(viewportChangedCallback) {
    this.viewportChangedCallback_ = viewportChangedCallback;
  }

  /**
   * @param {!Element} oopCompatObj The out of process compatibility element.
   * @return {boolean} Whether the plugin exists and is compatible.
   */
  checkPluginCompatibility(oopCompatObj) {
    return this.compatible_;
  }

  /** @return {boolean} Whether the plugin is ready. */
  pluginReady() {
    return !!this.fakePlugin_;
  }

  /**
   * Sets the load callback to imitate the plugin.
   * @param {number} previewUid The unique ID of the preview UI.
   * @param {number} index The preview index to load.
   * @return {?print_preview.PDFPlugin}
   */
  createPlugin(previewUid, index) {
    if (!this.compatible_) {
      return null;
    }

    this.fakePlugin_ = document.createElement('div');
    this.fakePlugin_.classList.add('preview-area-plugin');
    this.fakePlugin_.id = 'pdf-viewer';
    return this.fakePlugin_;
  }

  /**
   * @param {number} previewUid Unique identifier of preview.
   * @param {number} index Page index for plugin.
   * @param {boolean} color Whether the preview should be color.
   * @param {!Array<number>} pages Page indices to preview.
   * @param {boolean} modifiable Whether the document is modifiable.
   */
  resetPrintPreviewMode(previewUid, index, color, pages, modifiable) {}

  /**
   * @param {number} scrollX The amount to horizontally scroll in pixels.
   * @param {number} scrollY The amount to vertically scroll in pixels.
   */
  scrollPosition(scrollX, scrollY) {}

  /** @param {!KeyEvent} e Keyboard event to forward to the plugin. */
  sendKeyEvent(e) {}

  hideToolbars() {}

  /**
   * @param {boolean} eventsOn Whether pointer events should be captured by
   *     the plugin.
   */
  setPointerEvents(eventsOn) {}

  /**
   * Called when the preview area wants the plugin to load a preview page.
   * Immediately calls loadCallback_().
   * @param {number} previewUid The unique ID of the preview UI.
   * @param {number} pageIndex The page index to load.
   * @param {number} index The preview index.
   */
  loadPreviewPage(previewUid, pageIndex, index) {
    this.methodCalled(
        'loadPreviewPage',
        {previewUid: previewUid, pageIndex: pageIndex, index: index});
    if (this.preloadCallback_) {
      this.preloadCallback_();
    }
    if (this.loadCallback_) {
      this.loadCallback_(true);
    }
  }

  darkModeChanged(darkMode) {}

  /**
   * @param {number} pageX The horizontal offset for the page corner in
   *     pixels.
   * @param {number} pageY The vertical offset for the page corner in pixels.
   * @param {number} pageWidth The page width in pixels.
   * @param {number} viewportWidth The viewport width in pixels.
   * @param {number} viewportHeight The viewport height in pixels.
   * @private
   */
  triggerVisualStateChange(
      pageX, pageY, pageWidth, viewportWidth, viewportHeight) {
    this.viewportChangedCallback_.apply(this, arguments);
  }
}
