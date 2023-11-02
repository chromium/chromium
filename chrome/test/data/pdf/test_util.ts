// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utilities that are used in multiple tests.

import {DocumentDimensions, LayoutOptions, Viewport} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class MockElement {
  dir: string = '';
  offsetWidth: number;
  offsetHeight: number;
  sizer: MockSizer|null;
  scrollLeft: number = 0;
  scrollTop: number = 0;
  scrollCallback: (() => void)|null = null;
  resizeCallback: (() => void)|null = null;

  constructor(width: number, height: number, sizer: MockSizer|null) {
    this.offsetWidth = width;
    this.offsetHeight = height;
    this.sizer = sizer;

    if (sizer) {
      sizer.resizeCallback_ = () =>
          this.scrollTo(this.scrollLeft, this.scrollTop);
    }
  }

  get clientWidth(): number {
    return this.offsetWidth;
  }

  get clientHeight(): number {
    return this.offsetHeight;
  }

  addEventListener(e: string, f: () => void) {
    if (e === 'scroll') {
      this.scrollCallback = f;
    }
  }

  setSize(width: number, height: number) {
    this.offsetWidth = width;
    this.offsetHeight = height;
    this.resizeCallback!();
  }

  scrollTo(x: number, y: number) {
    if (this.sizer) {
      x = Math.min(x, parseInt(this.sizer.style.width, 10) - this.offsetWidth);
      y = Math.min(
          y, parseInt(this.sizer.style.height, 10) - this.offsetHeight);
    }
    this.scrollLeft = Math.max(0, x);
    this.scrollTop = Math.max(0, y);
    this.scrollCallback!();
  }
}

export class MockSizer {
  private width_: string = '0px';
  private height_: string = '0px';

  resizeCallback_: (() => void)|null = null;
  style: {
    height: string,
    width: string,
    display?: string,
  };

  constructor() {
    const sizer = this;

    this.style = {
      get height() {
        return sizer.height_;
      },

      set height(height: string) {
        sizer.height_ = height;
        if (sizer.resizeCallback_) {
          sizer.resizeCallback_();
        }
      },

      get width() {
        return sizer.width_;
      },

      set width(width: string) {
        sizer.width_ = width;
        if (sizer.resizeCallback_) {
          sizer.resizeCallback_();
        }
      },
    };
  }
}

export class MockViewportChangedCallback {
  wasCalled: boolean = false;
  callback: () => void;

  constructor() {
    this.callback = this.callback_.bind(this);
  }

  private callback_() {
    this.wasCalled = true;
  }

  reset() {
    this.wasCalled = false;
  }
}

interface PageDimensions {
  x: number;
  y: number;
  width: number;
  height: number;
}

export class MockDocumentDimensions implements DocumentDimensions {
  width: number;
  height: number;
  layoutOptions?: LayoutOptions;
  pageDimensions: PageDimensions[] = [];

  constructor(width?: number, height?: number, layoutOptions?: LayoutOptions) {
    this.width = width || 0;
    this.height = height || 0;
    this.layoutOptions = layoutOptions;
  }

  addPage(w: number, h: number) {
    let y = 0;
    if (this.pageDimensions.length !== 0) {
      y = this.pageDimensions[this.pageDimensions.length - 1]!.y +
          this.pageDimensions[this.pageDimensions.length - 1]!.height;
    }
    this.width = Math.max(this.width, w);
    this.height += h;
    this.pageDimensions.push({x: 0, y: y, width: w, height: h});
  }

  addPageForTwoUpView(x: number, y: number, w: number, h: number) {
    this.width = Math.max(this.width, 2 * w);
    this.height = Math.max(this.height, y + h);
    this.pageDimensions.push({x: x, y: y, width: w, height: h});
  }

  reset() {
    this.width = 0;
    this.height = 0;
    this.pageDimensions = [];
  }
}

export class MockUnseasonedPdfPluginElement extends HTMLEmbedElement {
  private messages_: any[] = [];

  get messages(): any[] {
    return this.messages_;
  }

  clearMessages() {
    this.messages_.length = 0;
  }

  findMessage(type: string): any {
    return this.messages_.find(element => element.type === type);
  }

  postMessage(message: any, _transfer: Transferable[]) {
    this.messages_.push(message);
  }
}
customElements.define(
    'mock-unseasoned-pdf-plugin', MockUnseasonedPdfPluginElement,
    {extends: 'embed'});

/**
 * Creates a fake element simulating the unseasoned PDF plugin.
 */
export function createMockUnseasonedPdfPluginForTest():
    MockUnseasonedPdfPluginElement {
  return document.createElement('embed', {is: 'mock-unseasoned-pdf-plugin'}) as
      MockUnseasonedPdfPluginElement;
}

/**
 * @return An element containing a dom-repeat of bookmarks, for
 *     testing the bookmarks outside of the toolbar.
 */
export function createBookmarksForTest(): HTMLElement {
  Polymer({
    is: 'test-bookmarks',

    _template: html`
      <template is="dom-repeat" items="[[bookmarks]]">
        <viewer-bookmark bookmark="[[item]]" depth="0"></viewer-bookmark>
      </template>`,

    properties: {
      bookmarks: Array,
    },
  });
  return document.createElement('test-bookmarks');
}

/**
 * Create a viewport with basic default zoom values.
 * @param sizer The element which represents the size of the document in the
 *     viewport.
 * @param scrollbarWidth The width of scrollbars on the page
 * @param defaultZoom The default zoom level.
 * @return The viewport object with zoom values set.
 */
export function getZoomableViewport(
    scrollParent: MockElement, sizer: MockSizer, scrollbarWidth: number,
    defaultZoom: number): Viewport {
  document.body.innerHTML = '';
  const dummyContent = document.createElement('div');
  document.body.appendChild(dummyContent);

  const viewport = new Viewport(
      scrollParent as unknown as HTMLElement, sizer as unknown as HTMLElement,
      dummyContent, scrollbarWidth, defaultZoom);
  viewport.setZoomFactorRange([0.25, 0.4, 0.5, 1, 2]);

  const dummyPlugin = document.createElement('embed');
  dummyPlugin.id = 'plugin';
  dummyPlugin.src = 'data:text/plain,plugin-content';
  viewport.setContent(dummyPlugin);
  return viewport;
}

/**
 * Async spin until predicate() returns true.
 */
export function waitFor(predicate: () => boolean): Promise<void> {
  if (predicate()) {
    return Promise.resolve();
  }
  return new Promise(resolve => setTimeout(() => {
                       resolve(waitFor(predicate));
                     }, 0));
}

export function createWheelEvent(
    deltaY: number, position: {clientX: number, clientY: number},
    ctrlKey: boolean): WheelEvent {
  return new WheelEvent('wheel', {
    deltaY,
    clientX: position.clientX,
    clientY: position.clientY,
    ctrlKey,
    // Necessary for preventDefault() to work.
    cancelable: true,
  });
}
