// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as animation from './animation.js';
import * as dom from './dom.js';
import {I18nString} from './i18n_string.js';
import * as loadTimeData from './models/load_time_data.js';
import * as util from './util.js';

/**
 * Interval of emerge time between two consecutive ripples in milliseconds.
 */
const RIPPLE_INTERVAL_MS = 5000;

/**
 * Controller for showing ripple effect.
 */
class RippleEffect {
  /**
   * @param {!HTMLElement} el Element to show ripple effect on.
   */
  constructor(el) {
    /**
     * @const {!HTMLElement}
     * @private
     */
    this.el_ = el;

    const style = this.el_.computedStyleMap();

    /**
     * Initial width of ripple in px.
     * @const {!number}
     * @private
     */
    this.width_ = util.getStyleValueInPx(style, '--ripple-start-width');

    /**
     * Initial height of ripple in px.
     * @const {!number}
     * @private
     */
    this.height_ = util.getStyleValueInPx(style, '--ripple-start-height');

    /**
     * @const {number}
     * @private
     */
    this.cancelHandle_ = setInterval(() => {
      this.addRipple_();
    }, RIPPLE_INTERVAL_MS);

    this.addRipple_();
  }

  /**
   * @return {!Promise}
   * @private
   */
  async addRipple_() {
    const rect = this.el_.getBoundingClientRect();
    if (rect.width === 0) {
      return;
    }
    const tpl = util.instantiateTemplate('#ripple-template');
    const ripple = dom.getFrom(tpl, '.ripple', HTMLDivElement);
    const style = ripple.attributeStyleMap;
    style.set('left', CSS.px(rect.left - (this.width_ - rect.width) / 2));
    style.set('top', CSS.px(rect.top - (this.height_ - rect.height) / 2));
    style.set('width', CSS.px(this.width_));
    style.set('height', CSS.px(this.height_));
    document.body.appendChild(tpl);
    await animation.play(ripple);
    document.body.removeChild(ripple);
  }

  /**
   * Stops ripple effect.
   * @public
   */
  stop() {
    clearInterval(this.cancelHandle_);
  }
}

/**
 * Interval for toast updaing position.
 */
const TOAST_POSITION_UPDATE_MS = 500;

/**
 * @enum {string}
 */
const PositionProperty = {
  BOTTOM: 'bottom',
  LEFT: 'left',
  RIGHT: 'right',
  TOP: 'top',
};

/**
 * Controller for showing new feature toast.
 */
class Toast {
  /**
   * @param {!HTMLElement} el
   */
  constructor(el) {
    /**
     * @const {!HTMLElement}
     * @private
     */
    this.el_ = el;

    /**
     * Offset between the position property of toast and the target |el| to
     * determine their relative position.
     * @const {
     *   !Array<{
     *     elProperty: !PositionProperty,
     *     toastProperty: !PositionProperty,
     *     offset: number,
     *   }>}
     * @private
     */
    this.offsetProperties_ = (() => {
      const properties = [];
      const style = this.el_.computedStyleMap();
      for (const dir of ['x', 'y']) {
        const toastProperty = style.get(`--toast-ref-${dir}`).toString();
        const elProperty = style.get(`--toast-element-ref-${dir}`).toString();
        const offset = util.getStyleValueInPx(style, `--toast-offset-${dir}`);
        properties.push({elProperty, toastProperty, offset});
      }
      return properties;
    })();

    const tpl = util.instantiateTemplate('#new-feature-toast-template');

    /**
     * @const {!HTMLDivElement}
     * @private
     */
    this.toast_ = dom.getFrom(tpl, '#new-feature-toast', HTMLDivElement);

    /**
     * @const {number}
     * @private
     */
    this.cancelHandle_ = setInterval(() => {
      this.updatePosition_();
    }, TOAST_POSITION_UPDATE_MS);

    // Set up i18n texts.
    const i18nId =
        /** @type {!I18nString} */ (el.getAttribute('i18n-new-feature'));
    const textEl = dom.getFrom(tpl, '.new-feature-toast-text', HTMLSpanElement);
    const text = loadTimeData.getI18nMessage(i18nId);
    textEl.textContent = text;
    const ariaLabel =
        loadTimeData.getI18nMessage(I18nString.NEW_CONTROL_NAVIGATION, text);
    this.toast_.setAttribute('aria-label', ariaLabel);

    document.body.appendChild(tpl);
    this.updatePosition_();
  }

  /**
   * @private
   */
  updatePosition_() {
    const rect = this.el_.getBoundingClientRect();
    const style = this.toast_.attributeStyleMap;
    if (rect.width === 0) {
      style.set('display', 'none');
      return;
    }
    style.clear();
    for (const {elProperty, toastProperty, offset} of this.offsetProperties_) {
      let value = rect[elProperty] + offset;
      if (toastProperty === PositionProperty.RIGHT) {
        value = window.innerWidth - value;
      } else if (toastProperty === PositionProperty.BOTTOM) {
        value = window.innerHeight - value;
      }
      style.set(toastProperty, CSS.px(value));
    }
  }

  /**
   * @public
   */
  focus() {
    this.el_.setAttribute('aria-owns', 'new-feature-toast');
    this.toast_.focus();
  }

  /**
   * @public
   */
  hide() {
    this.el_.removeAttribute('aria-owns');
    clearInterval(this.cancelHandle_);
    document.body.removeChild(this.toast_);
  }
}

/**
 * @type {?{
 *   ripple: !RippleEffect,
 *   toast: !Toast,
 *   timeout: number,
 * }}
 */
let showing = null;

/**
 * @public
 */
export function hide() {
  if (showing === null) {
    return;
  }
  const {ripple, toast, timeout} = showing;
  showing = null;
  ripple.stop();
  toast.hide();
  clearTimeout(timeout);
}

/**
 * Timeout for showing new feature toast.
 */
const SHOWING_TIMEOUT_MS = 10000;

/**
 * @param {!HTMLElement} el
 * @return {!Promise}
 */
export async function show(el) {
  if (showing !== null) {
    hide();
  }

  await new Promise((r) => {
    const ripple = new RippleEffect(el);
    const toast = new Toast(el);
    const timeout = setTimeout(r, SHOWING_TIMEOUT_MS);
    showing = {ripple, toast, timeout};
  });
  hide();
}

/**
 * @return {boolean} If new feature toast is showing.
 */
export function isShowing() {
  return showing !== null;
}

/**
 * Focuses to new feature toast.
 */
export function focus() {
  if (showing === null) {
    return;
  }
  showing.toast.focus();
}
