// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PdfPlugin} from 'chrome://print/pdf/pdf_scripting_api.js';
import type {PluginProxy, ViewportChangedCallback} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/**
 * Test version of the PluginProxy.
 */
export class TestPluginProxy extends TestBrowserProxy implements PluginProxy {
  private loadCompleteCallback_: ((success: boolean) => void)|null = null;
  private preloadCallback_: (() => void)|null = null;
  private viewportChangedCallback_: ViewportChangedCallback|null = null;
  private fakePlugin_: HTMLDivElement|null = null;

  constructor() {
    super(['loadPreviewPage']);
  }

  setLoadCompleteCallback(loadCompleteCallback: (success: boolean) => void) {
    assert(!this.loadCompleteCallback_);
    this.loadCompleteCallback_ = loadCompleteCallback;
  }

  setPreloadCallback(preloadCallback: (() => void)|null) {
    this.preloadCallback_ = preloadCallback;
  }

  setKeyEventCallback(_keyEventCallback: (e: KeyboardEvent) => void) {}

  setViewportChangedCallback(viewportChangedCallback: ViewportChangedCallback) {
    this.viewportChangedCallback_ = viewportChangedCallback;
  }

  pluginReady() {
    return !!this.fakePlugin_;
  }

  createPlugin(_previewUid: number, _index: number) {
    this.fakePlugin_ = document.createElement('div');
    this.fakePlugin_.classList.add('preview-area-plugin');
    this.fakePlugin_.id = 'pdf-viewer';
    return this.fakePlugin_ as unknown as PdfPlugin;
  }

  resetPrintPreviewMode(
      _previewUid: number, _index: number, _color: boolean, _pages: number[],
      _modifiable: boolean) {}

  scrollPosition(_scrollX: number, _scrollY: number) {}

  sendKeyEvent(_e: KeyboardEvent) {}

  hideToolbar() {}

  setPointerEvents(_eventsOn: boolean) {}

  loadPreviewPage(previewUid: number, pageIndex: number, index: number) {
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

  darkModeChanged(_darkMode: boolean) {}

  /**
   * @param pageX The horizontal offset for the page corner in pixels.
   * @param pageY The vertical offset for the page corner in pixels.
   * @param pageWidth The page width in pixels.
   * @param viewportWidth The viewport width in pixels.
   * @param viewportHeight The viewport height in pixels.
   */
  triggerVisualStateChange(
      pageX: number, pageY: number, pageWidth: number, viewportWidth: number,
      viewportHeight: number) {
    this.viewportChangedCallback_!
        (pageX, pageY, pageWidth, viewportWidth, viewportHeight);
  }
}
