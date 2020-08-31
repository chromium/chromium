// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxy} from '../../browser_proxy/browser_proxy.js';
import {
  assert,
  assertInstanceof,
} from '../../chrome_util.js';
import * as dom from '../../dom.js';
import * as state from '../../state.js';
import {Mode, Resolution} from '../../type.js';

/**
 * CSS rules.
 * @type {!Array<!CSSStyleRule>}
 */
const cssRules = (() => {
  const sheet = assertInstanceof(document.styleSheets[0], CSSStyleSheet);
  const ruleList = /** @type {!Iterable} */ (sheet.cssRules);
  return [...ruleList];
})();

/**
 * Creates a controller to handle layouts of Camera view.
 */
export class Layout {
  /**
   * @public
   */
  constructor() {
    /**
     * CSS style of the viewport in square mode.
     * @type {!CSSStyleDeclaration}
     * @private
     */
    this.squareViewport_ =
        this.constructor.cssStyle_('body.square-preview #preview-wrapper');

    /**
     * CSS style of the video in square mode.
     * @type {!CSSStyleDeclaration}
     * @private
     */
    this.squareVideo_ =
        this.constructor.cssStyle_('body.square-preview .preview-content');

    /**
     * CSS style of what is currently put as camera preview.
     * @type {!CSSStyleDeclaration}
     * @private
     */
    this.previewContent_ = this.constructor.cssStyle_('.preview-content');
  }

  /**
   * Gets the CSS style by the given selector.
   * @param {string} selector Selector text.
   * @return {!CSSStyleDeclaration}
   * @private
   */
  static cssStyle_(selector) {
    const rule = cssRules.find((rule) => rule.selectorText === selector);
    assert(rule !== undefined);
    assert(rule.style !== null);
    return rule.style;
  }

  /**
   * Updates the video element size for previewing in the window.
   * @return {!Resolution} Letterbox size.
   * @private
   */
  updatePreviewSize_() {
    // Make video content keeps its aspect ratio inside the window's
    // inner-bounds; it may fill up the window or be letterboxed when
    // fullscreen/maximized. Don't use app-window.innerBounds' width/height
    // properties during resizing as they are not updated immediately.
    const video = dom.get('#preview-video', HTMLVideoElement);
    let contentWidth = 0;
    let contentHeight = 0;
    if (video.videoHeight) {
      const scale = state.get(Mode.SQUARE) ?
          Math.min(window.innerHeight, window.innerWidth) /
              Math.min(video.videoHeight, video.videoWidth) :
          Math.min(
              window.innerHeight / video.videoHeight,
              window.innerWidth / video.videoWidth);
      contentWidth = scale * video.videoWidth;
      contentHeight = scale * video.videoHeight;
      this.previewContent_.setProperty('width', `${contentWidth}px`);
      this.previewContent_.setProperty('height', `${contentHeight}px`);
    }
    let viewportW = contentWidth;
    let viewportH = contentHeight;
    state.set(state.State.SQUARE_PREVIEW, state.get(Mode.SQUARE));
    if (state.get(Mode.SQUARE)) {
      viewportW = viewportH = Math.min(contentWidth, contentHeight);
      this.squareVideo_.setProperty(
          'left', `${(viewportW - contentWidth) / 2}px`);
      this.squareVideo_.setProperty(
          'top', `${(viewportH - contentHeight) / 2}px`);
      this.squareViewport_.setProperty('width', `${viewportW}px`);
      this.squareViewport_.setProperty('height', `${viewportH}px`);
    }
    return new Resolution(
        window.innerWidth - viewportW, window.innerHeight - viewportH);
  }

  /**
   * Updates the layout for video-size or window-size changes.
   */
  update() {
    const fullWindow = browserProxy.isFullscreenOrMaximized();
    const tall = window.innerHeight > window.innerWidth;
    state.set(state.State.TABLET_LANDSCAPE, fullWindow && !tall);
    state.set(state.State.MAX_WND, fullWindow);
    state.set(state.State.TALL, tall);

    const {width: letterboxW, height: letterboxH} = this.updatePreviewSize_();
    const isLetterboxW = letterboxH < letterboxW;

    state.set(state.State.W_LETTERBOX, isLetterboxW);
    if (isLetterboxW) {
      const modeWidth =
          document.querySelector('#modes-group').getBoundingClientRect().width;
      let layoutToggled = false;
      [[modeWidth + 30, state.State.W_LETTERBOX_S],
       [modeWidth + 30 + 72, state.State.W_LETTERBOX_M],
       [(modeWidth + 30) * 2, state.State.W_LETTERBOX_L],
       [Infinity, state.State.W_LETTERBOX_XL],
      ]
          .forEach(
              ([wSize, classname]) => state.set(
                  classname,
                  /* Enable only state which the letterboxW size falls in range
                   * of its wSize and previous wSize. And disable all other
                   * states. */
                  !layoutToggled && (layoutToggled = letterboxW <= wSize)));
    } else {
      // preview-vertical-dock: Dock bottom line of preview between gallery and
      //                        mode selector.
      // otherwise: Vertically center the preview.
      state.set(state.State.PREVIEW_VERTICAL_DOCK, letterboxH / 2 >= 112);
    }
  }
}
