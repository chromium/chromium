// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertString} from '../chrome_util.js';
import * as dom from '../dom.js';
import {ViewName} from '../type.js';  // eslint-disable-line no-unused-vars

import {View} from './view.js';

/**
 * Creates the Dialog view controller.
 */
export class Dialog extends View {
  /**
   * @param {!ViewName} name View name of the dialog.
   */
  constructor(name) {
    super(name, true);

    /**
     * @type {!HTMLButtonElement}
     * @private
     */
    this.positiveButton_ =
        dom.getFrom(this.root, '.dialog-positive-button', HTMLButtonElement);

    /**
     * @type {!HTMLButtonElement}
     * @private
     */
    this.negativeButton_ =
        dom.getFrom(this.root, '.dialog-negative-button', HTMLButtonElement);

    /**
     * @type {!HTMLElement}
     * @private
     */
    this.messageHolder_ =
        dom.getFrom(this.root, '.dialog-msg-holder', HTMLElement);

    this.positiveButton_.addEventListener('click', () => this.leave(true));
    if (this.negativeButton_) {
      this.negativeButton_.addEventListener('click', () => this.leave());
    }
  }

  /**
   * @override
   */
  entering({message, cancellable = false} = {}) {
    message = assertString(message);
    this.messageHolder_.textContent = message;
    if (this.negativeButton_) {
      this.negativeButton_.hidden = !cancellable;
    }
  }

  /**
   * @override
   */
  focus() {
    this.positiveButton_.focus();
  }
}
