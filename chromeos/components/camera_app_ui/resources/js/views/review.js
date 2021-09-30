// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as dom from '../dom.js';
// eslint-disable-next-line no-unused-vars
import {I18nString} from '../i18n_string.js';
import * as nav from '../nav.js';
import {ViewName} from '../type.js';
import {instantiateTemplate, setupI18nElements} from '../util.js';
import {WaitableEvent} from '../waitable_event.js';

import {View} from './view.js';

/**
 * Available option show in this view.
 * @template T
 */
export class Option {
  /**
   * @param {!I18nString} text Text string show on the option button.
   * @param {{
   *   exitValue: (!T|undefined),
   *   callback: (function()|undefined),
   * }} handlerParams Sets |exitValue| if the review page will exit with this
   *   value when option selected. Sets |callback| for the function get executed
   *   when option selected.
   */
  constructor(text, {exitValue, callback}) {
    /**
     * @const {!I18nString}
     */
    this.text = text;

    /**
     * @const {?T}
     */
    this.exitValue = exitValue ?? null;

    /**
     * @const {?function()}
     */
    this.callback = callback || null;
  }
}

/**
 * Options for reviewing.
 * @template T
 */
export class Options {
  /**
   * @param {!Option} primary
   * @param {...!Option} others
   */
  constructor(primary, ...others) {
    /**
     * @const {!Option<!T>}
     */
    this.primary = primary;

    /**
     * @const {!Array<!Option<!T>>}
     */
    this.others = others;
  }
}

/**
 * View controller for review page.
 */
export class Review extends View {
  /**
   * @public
   */
  constructor() {
    super(ViewName.REVIEW);

    /**
     * @private {!HTMLImageElement}
     * @const
     */
    this.image_ = dom.get('#review-image', HTMLImageElement);

    /**
     * @private {!HTMLDivElement}
     * @const
     */
    this.btnGroups_ =
        dom.getFrom(this.root, '.positive-button-groups', HTMLDivElement);

    /**
     * @private {!HTMLButtonElement}
     * @const
     */
    this.retakeBtn_ = dom.get('#review-retake', HTMLButtonElement);

    /**
     * @private {?HTMLButtonElement}
     */
    this.primaryBtn_ = null;
  }

  /**
   * @override
   */
  focus() {
    this.primaryBtn_.focus();
  }

  /**
   * @param {!Blob} blob
   * @return {!Promise}
   */
  async setReviewPhoto(blob) {
    try {
      await new Promise((resolve, reject) => {
        this.image_.onload = () => resolve();
        this.image_.onerror = (e) =>
            reject(new Error('Failed to load review document image: ${e}'));
        this.image_.src = URL.createObjectURL(blob);
      });
    } finally {
      URL.revokeObjectURL(this.image_.src);
    }
  }

  /**
   * @template T
   * @param {!Options} options
   * @return {!Promise<?T>}
   */
  async startReview({primary, others = []}) {
    // Remove all existing buttons.
    while (this.btnGroups_.firstChild) {
      this.btnGroups_.removeChild(this.btnGroups_.lastChild);
    }

    const onSelected = new WaitableEvent();
    /**
     * @param {!Option<!T>} option
     * @param {boolean} isPrimary
     */
    const addButton = ({text, exitValue, callback}, isPrimary) => {
      const templ = instantiateTemplate('#text-button-template');
      const btn = dom.getFrom(templ, 'button', HTMLButtonElement);
      btn.setAttribute('i18n-text', text);
      if (isPrimary) {
        btn.classList.add('primary');
        this.primaryBtn_ = btn;
      } else {
        btn.classList.add('secondary');
      }
      btn.onclick = () => {
        if (callback !== null) {
          callback();
        }
        if (exitValue !== null) {
          onSelected.signal(exitValue);
        }
      };
      this.btnGroups_.appendChild(templ);
    };
    addButton(primary, true);
    for (const opt of others) {
      addButton(opt, false);
    }
    this.retakeBtn_.onclick = () => {
      onSelected.signal(null);
    };
    setupI18nElements(this.btnGroups_);

    nav.open(ViewName.REVIEW);
    const result = await onSelected.wait();
    nav.close(ViewName.REVIEW);
    this.image_.src = '';
    return result;
  }
}
