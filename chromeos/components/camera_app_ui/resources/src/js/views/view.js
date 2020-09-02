// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as dom from '../dom.js';
import {ViewName} from '../type.js';  // eslint-disable-line no-unused-vars
import {WaitableEvent} from '../waitable_event.js';

/* eslint-disable no-unused-vars */

/**
 * message for message of the dialog view, cancellable for whether the dialog
 * view is cancellable.
 * @typedef {{
 *   message: string,
 *   cancellable: (boolean|undefined),
 * }}
 */
let DialogEnterOptions;

/**
 * Warning message name.
 * @typedef {string}
 */
let WarningEnterOptions;

/**
 * @typedef {!DialogEnterOptions|!WarningEnterOptions}
 */
let EnterOptions;

/* eslint-enable no-unused-vars */

/**
 * Base controller of a view for views' navigation sessions (nav.js).
 */
export class View {
  /**
   * @param {!ViewName} name Unique name of view which should be same as its DOM
   *     element id.
   * @param {boolean=} dismissByEsc Enable dismissible by Esc-key.
   * @param {boolean=} dismissByBkgndClick Enable dismissible by
   *     background-click.
   */
  constructor(name, dismissByEsc = false, dismissByBkgndClick = false) {
    /**
     * @const {!ViewName}
     */
    this.name = name;

    /**
     * @type {!HTMLElement}
     * @protected
     */
    this.rootElement_ = dom.get(`#${name}`, HTMLElement);

    /**
     * Signal it to ends the session.
     * @type {?WaitableEvent<*>}
     * @private
     */
    this.session_ = null;

    /**
     * @type {boolean}
     * @private
     */
    this.dismissByEsc_ = dismissByEsc;

    if (dismissByBkgndClick) {
      this.rootElement_.addEventListener(
          'click',
          (event) =>
              event.target === this.rootElement_ && this.leave({bkgnd: true}));
    }
  }

  /**
   * HTML root node of this view.
   * @return {!HTMLElement}
   */
  get root() {
    return this.rootElement_;
  }

  /**
   * Hook of the subclass for handling the key.
   * @param {string} key Key to be handled.
   * @return {boolean} Whether the key has been handled or not.
   */
  handlingKey(key) {
    return false;
  }

  /**
   * Handles the pressed key.
   * @param {string} key Key to be handled.
   * @return {boolean} Whether the key has been handled or not.
   */
  onKeyPressed(key) {
    if (this.handlingKey(key)) {
      return true;
    } else if (this.dismissByEsc_ && key === 'Escape') {
      this.leave();
      return true;
    }
    return false;
  }

  /**
   * Focuses the default element on the view if applicable.
   */
  focus() {}

  /**
   * Layouts the view.
   */
  layout() {}

  /**
   * Hook of the subclass for entering the view.
   * @param {!EnterOptions=} options Optional rest parameters for
   *     entering the view.
   */
  entering(options) {}

  /**
   * Enters the view.
   * @param {!EnterOptions=} options Optional rest parameters for
   *     entering the view.
   * @return {!Promise<*>} Promise for the navigation session.
   */
  enter(options) {
    // The session is started by entering the view and ended by leaving the
    // view.
    if (this.session_ === null) {
      this.session_ = new WaitableEvent();
    }
    this.entering(options);
    return this.session_.wait();
  }

  /**
   * Hook of the subclass for leaving the view.
   * @param {*=} condition Optional condition for leaving the view.
   * @return {boolean} Whether able to leaving the view or not.
   */
  leaving(condition) {
    return true;
  }

  /**
   * Leaves the view.
   * @param {*=} condition Optional condition for leaving the view and also as
   *     the result for the ended session.
   * @return {boolean} Whether able to leaving the view or not.
   */
  leave(condition) {
    if (this.session_ !== null && this.leaving(condition)) {
      this.session_.signal(condition);
      this.session_ = null;
      return true;
    }
    return false;
  }
}
