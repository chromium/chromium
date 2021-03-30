// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utilities that are used in multiple tests.

import {LayoutOptions, Viewport} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class MockElement {
  /**
   * @param {number} width
   * @param {number} height
   * @param {?MockSizer} sizer
   */
  constructor(width, height, sizer) {
    /** @type {number} */
    this.offsetWidth = width;

    /** @type {number} */
    this.offsetHeight = height;

    /** @type {?MockSizer} */
    this.sizer = sizer;

    if (sizer) {
      sizer.resizeCallback_ = () =>
          this.scrollTo(this.scrollLeft, this.scrollTop);
    }

    /** @type {number} */
    this.scrollLeft = 0;

    /** @type {number} */
    this.scrollTop = 0;

    /** @type {?Function} */
    this.scrollCallback = null;

    /** @type {?Function} */
    this.resizeCallback = null;
  }

  /**
   * @param {string} e The event name
   * @param {!Function} f The callback
   */
  addEventListener(e, f) {
    if (e === 'scroll') {
      this.scrollCallback = f;
    }
  }

  /**
   * @param {number} width
   * @param {number} height
   */
  setSize(width, height) {
    this.offsetWidth = width;
    this.offsetHeight = height;
    this.resizeCallback();
  }

  /**
   * @param {number} x
   * @param {number} y
   */
  scrollTo(x, y) {
    if (this.sizer) {
      x = Math.min(x, parseInt(this.sizer.style.width, 10) - this.offsetWidth);
      y = Math.min(
          y, parseInt(this.sizer.style.height, 10) - this.offsetHeight);
    }
    this.scrollLeft = Math.max(0, x);
    this.scrollTop = Math.max(0, y);
    this.scrollCallback();
  }
}

export class MockSizer {
  constructor() {
    const sizer = this;

    /** @private {?Function} */
    this.resizeCallback_ = null;

    this.style = {
      width_: '0px',
      height_: '0px',
      get height() {
        return this.height_;
      },
      set height(height) {
        this.height_ = height;
        if (sizer.resizeCallback_) {
          sizer.resizeCallback_();
        }
      },
      get width() {
        return this.width_;
      },
      set width(width) {
        this.width_ = width;
        if (sizer.resizeCallback_) {
          sizer.resizeCallback_();
        }
      },
    };
  }
}

export class MockViewportChangedCallback {
  constructor() {
    /** @type {boolean} */
    this.wasCalled = false;

    /** @type {!Function} */
    this.callback = () => {
      this.wasCalled = true;
    };
  }

  reset() {
    this.wasCalled = false;
  }
}

export class MockDocumentDimensions {
  /**
   * @param {number=} width
   * @param {number=} height
   * @param {LayoutOptions=} layoutOptions
   */
  constructor(width, height, layoutOptions) {
    /** @type {number} */
    this.width = width || 0;

    /** @type {number} */
    this.height = height ? height : 0;

    /** @type {(LayoutOptions|undefined)} */
    this.layoutOptions = layoutOptions;

    /** @type {!Array<{x: number, y: number, width: number, height: number}>} */
    this.pageDimensions = [];
  }

  /**
   * @param {number} w
   * @param {number} h
   */
  addPage(w, h) {
    let y = 0;
    if (this.pageDimensions.length !== 0) {
      y = this.pageDimensions[this.pageDimensions.length - 1].y +
          this.pageDimensions[this.pageDimensions.length - 1].height;
    }
    this.width = Math.max(this.width, w);
    this.height += h;
    this.pageDimensions.push({x: 0, y: y, width: w, height: h});
  }

  /**
   * @param {number} x
   * @param {number} y
   * @param {number} w
   * @param {number} h
   */
  addPageForTwoUpView(x, y, w, h) {
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

/**
 * @return {!Element} An element containing a dom-repeat of bookmarks, for
 *     testing the bookmarks outside of the toolbar.
 */
export function createBookmarksForTest() {
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
 * @param {(!MockElement|!HTMLElement)} scrollParent
 * @param {(!MockSizer|!HTMLDivElement)} sizer The element which represents the
 *     size of the document in the viewport
 * @param {number} scrollbarWidth The width of scrollbars on the page
 * @param {number} defaultZoom The default zoom level.
 * @return {!Viewport} The viewport object with zoom values set.
 */
export function getZoomableViewport(
    scrollParent, sizer, scrollbarWidth, defaultZoom) {
  document.body.innerHTML = '';
  const dummyContent =
      /** @type {!HTMLDivElement} */ (document.createElement('div'));
  document.body.appendChild(dummyContent);
  const dummyPlugin =
      /** @type {!HTMLEmbedElement} */ (document.createElement('embed'));
  dummyPlugin.id = 'plugin';
  dummyContent.appendChild(dummyPlugin);
  const viewport = new Viewport(
      /** @type {!HTMLElement} */ (scrollParent),
      /** @type {!HTMLDivElement} */ (sizer), dummyContent, scrollbarWidth,
      defaultZoom);
  viewport.setZoomFactorRange([0.25, 0.4, 0.5, 1, 2]);
  return viewport;
}

/**
 * Async spin until predicate() returns true.
 * @param {function(): boolean} predicate
 * @return {!Promise|undefined}
 */
export function waitFor(predicate) {
  if (predicate()) {
    return;
  }
  return new Promise(resolve => setTimeout(() => {
                       resolve(waitFor(predicate));
                     }, 0));
}

/**
 * @param {number} deltaY
 * @param {{clientX: number, clientY: number}} position
 * @param {boolean} ctrlKey
 * @return {!WheelEvent}
 */
export function createWheelEvent(deltaY, position, ctrlKey) {
  return new WheelEvent('wheel', {
    deltaY,
    clientX: position.clientX,
    clientY: position.clientY,
    ctrlKey,
    // Necessary for preventDefault() to work.
    cancelable: true,
  });
}
