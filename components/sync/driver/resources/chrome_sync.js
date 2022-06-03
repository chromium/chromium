// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';

/**
 * A simple timer to measure elapsed time.
 */
export class Timer {
  constructor() {
    /**
     * The time that this Timer was created.
     * @type {number}
     * @private
     */
    this.start_ = Date.now();
  }

  /**
   * @return {number} The elapsed seconds since this Timer was created.
   */
  getElapsedSeconds() {
    return (Date.now() - this.start_) / 1000;
  }
}

  /**
   * Requests the sync state, which is sent via onAboutInfoUpdated and
   * onEntityCountsUpdated events. New events will be emitted whenever the
   * state changes.
   */
export function requestDataAndRegisterForUpdates() {
  chrome.send('requestDataAndRegisterForUpdates');
}


  /**
   * Asks the browser to send us the list of registered types. Should result
   * in an onReceivedListOfTypes event being emitted.
   */
export function requestListOfTypes() {
  chrome.send('requestListOfTypes');
}

  /**
   * Asks the browser to send us the initial state of the "include specifics"
   * flag. Should result in an onReceivedIncludeSpecificsInitialState event
   * being emitted.
   */
export function requestIncludeSpecificsInitialState() {
  chrome.send('requestIncludeSpecificsInitialState');
}

  /**
   * Updates the logic sending events to the protocol logic if they should
   * include specifics or not when converting to a human readable format.
   *
   * @param {boolean} includeSpecifics Whether protocol events include
   *     specifics.
   */
export function setIncludeSpecifics(includeSpecifics) {
  chrome.send('setIncludeSpecifics', [includeSpecifics]);
}

  /**
   * Sends data to construct a user event that should be committed.
   *
   * @param {string} eventTimeUsec Timestamp for the new event.
   * @param {string} navigationId Timestamp of linked sessions navigation.
   */
export function writeUserEvent(eventTimeUsec, navigationId) {
  chrome.send('writeUserEvent', [eventTimeUsec, navigationId]);
}

  /**
   * Triggers a RequestStart call on the SyncService.
   */
export function requestStart() {
  chrome.send('requestStart');
}

  /**
   * Stops the SyncService while keeping the sync data around.
   */
export function requestStopKeepData() {
  chrome.send('requestStopKeepData');
}

  /**
   * Stops the SyncService and clears the sync data.
   */
export function requestStopClearData() {
  chrome.send('requestStopClearData');
}

  /**
   * Triggers a GetUpdates call for all enabled datatypes.
   */
export function triggerRefresh() {
  chrome.send('triggerRefresh');
}

let nodesForTest = null;

/**
 * Asks the browser to send us a copy of all existing sync nodes.
 * Will eventually invoke the given callback with the results.
 *
 * @param {function(!Object)} callback The function to call with the response.
 */
export function getAllNodes(callback) {
  if (nodesForTest) {
    callback(nodesForTest);
    return;
  }
  sendWithPromise('getAllNodes').then(callback);
}

window.setAllNodesForTest = function(nodes) {
  nodesForTest = nodes;
};
