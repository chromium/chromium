// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxy} from '../browser_proxy/browser_proxy.js';
import {assertString} from '../chrome_util.js';
import {ViewName} from '../type.js';
import {View} from './view.js';

/**
 * Creates the warning-view controller.
 */
export class Warning extends View {
  /**
   * @public
   */
  constructor() {
    super(ViewName.WARNING);

    /**
     * @type {!Array<string>}
     * @private
     */
    this.errorNames_ = [];
  }

  /**
   * Updates the error message for the latest error-name in the stack.
   * @private
   */
  updateMessage_() {
    let message = '';
    switch (this.errorNames_[this.errorNames_.length - 1]) {
      case 'no-camera':
        message = 'error_msg_no_camera';
        break;
      case 'filesystem-failure':
        message = 'error_msg_file_system_failed';
        break;
    }
    document.querySelector('#error-msg').textContent =
        browserProxy.getI18nMessage(message);
  }

  /**
   * @override
   */
  entering(name) {
    name = assertString(name);

    // Remove the error-name from the stack to avoid duplication. Then make the
    // error-name the latest one to show its message.
    const index = this.errorNames_.indexOf(name);
    if (index !== -1) {
      this.errorNames_.splice(index, 1);
    }
    this.errorNames_.push(name);
    this.updateMessage_();
  }

  /**
   * @override
   */
  leaving(...args) {
    /**
     * Recovered error-name for leaving the view.
     * @type {string}
     */
    const name = assertString(args[0]);

    // Remove the recovered error from the stack but don't leave the view until
    // there is no error left in the stack.
    const index = this.errorNames_.indexOf(name);
    if (index !== -1) {
      this.errorNames_.splice(index, 1);
    }
    if (this.errorNames_.length) {
      this.updateMessage_();
      return false;
    }
    document.querySelector('#error-msg').textContent = '';
    return true;
  }
}
