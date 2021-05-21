// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addWebUIListener} from 'chrome://resources/js/cr.m.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';

/**
 * Creates a new log object which then immediately starts recording sync
 * protocol events.  Recorded entries are available in the 'entries'
 * property and there is an 'append' event which can be listened to.
 */
class Log extends EventTarget {
  constructor() {
    super();

    /**
     * Must match the value in SyncInternalsMessageHandler::OnProtocolEvent().
     * @private
     * @type {string}
     */
    this.protocolEventName_ = 'onProtocolEvent';

    /**
     * The recorded log entries.
     * @type {!Array}
     */
    this.entries = [];

    addWebUIListener(this.protocolEventName_, (response) => {
      this.log_(response);
    });
  }

  /**
   * Records a single event with the given parameters and fires the
   * 'append' event with the newly-created event as the 'detail'
   * field of a custom event.
   * @param {!Object} details A dictionary of event-specific details.
   */
  log_(details) {
    const entry = {
      submodule: 'protocol',
      event: this.protocolEventName_,
      date: new Date(),
      details: details,
      textDetails: ''
    };
    entry.textDetails = JSON.stringify(entry.details, null, 2);
    this.entries.push(entry);
    // Fire append event.
    const e = document.createEvent('CustomEvent');
    e.initCustomEvent('append', false, false, entry);
    this.dispatchEvent(e);
  }
}

export const log = new Log();
