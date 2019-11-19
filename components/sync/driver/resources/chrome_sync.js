// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// require cr.js
// require cr/util.js

cr.define('chrome.sync', function() {
  'use strict';

  /**
   * A simple timer to measure elapsed time.
   * @constructor
   */
  class Timer {
    constructor() {
      /**
       * The time that this Timer was created.
       * @type {number}
       * @private
       * @const
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
   * @param {string} name The name of the event type.
   * @param {!Object} details A collection of event-specific details.
   */
  function dispatchEvent(name, details) {
    const e = new Event(name);
    e.details = details;
    chrome.sync.events.dispatchEvent(e);
  }

  /**
   * Registers to receive a stream of events through
   * chrome.sync.dispatchEvent().
   */
  function registerForEvents() {
    chrome.send('registerForEvents');
  }

  /**
   * Registers to receive a stream of status counter update events
   * chrome.sync.dispatchEvent().
   */
  function registerForPerTypeCounters() {
    chrome.send('registerForPerTypeCounters');
  }

  /**
   * Asks the browser to refresh our snapshot of sync state. Should result
   * in an onAboutInfoUpdated event being emitted.
   */
  function requestUpdatedAboutInfo() {
    chrome.send('requestUpdatedAboutInfo');
  }

  /**
   * Asks the browser to send us the list of registered types. Should result
   * in an onReceivedListOfTypes event being emitted.
   */
  function requestListOfTypes() {
    chrome.send('requestListOfTypes');
  }

  /**
   * Asks the browser to send us the initial state of the "include specifics"
   * flag. Should result in an onReceivedIncludeSpecificsInitialState event
   * being emitted.
   */
  function requestIncludeSpecificsInitialState() {
    chrome.send('requestIncludeSpecificsInitialState');
  }

  /**
   * Asks the browser if we should show the User Events tab or not.
   */
  function requestUserEventsVisibility() {
    chrome.send('requestUserEventsVisibility');
  }

  /**
   * Updates the logic sending events to the protocol logic if they should
   * include specifics or not when converting to a human readable format.
   *
   * @param {boolean} includeSpecifics Whether protocol events include
   *     specifics.
   */
  function setIncludeSpecifics(includeSpecifics) {
    chrome.send('setIncludeSpecifics', [includeSpecifics]);
  }

  /**
   * Sends data to construct a user event that should be committed.
   *
   * @param {string} eventTimeUsec Timestamp for the new event.
   * @param {string} navigationId Timestamp of linked sessions navigation.
   */
  function writeUserEvent(eventTimeUsec, navigationId) {
    chrome.send('writeUserEvent', [eventTimeUsec, navigationId]);
  }

  /**
   * Triggers a RequestStart call on the SyncService.
   */
  function requestStart() {
    chrome.send('requestStart');
  }

  /**
   * Triggers a RequestStop(KEEP_DATA) call on the SyncService.
   */
  function requestStopKeepData() {
    chrome.send('requestStopKeepData');
  }

  /**
   * Triggers a RequestStop(CLEAR_DATA) call on the SyncService.
   */
  function requestStopClearData() {
    chrome.send('requestStopClearData');
  }

  /**
   * Triggers a GetUpdates call for all enabled datatypes.
   */
  function triggerRefresh() {
    chrome.send('triggerRefresh');
  }

  /**
   * Counter to uniquely identify requests while they're in progress.
   * Used in the implementation of GetAllNodes.
   */
  let requestId = 0;

  /**
   * A map from counter values to asynchronous request callbacks.
   * Used in the implementation of GetAllNodes.
   * @type {!Object<!Function>}
   */
  const requestCallbacks = {};

  /**
   * Asks the browser to send us a copy of all existing sync nodes.
   * Will eventually invoke the given callback with the results.
   *
   * @param {function(!Object)} callback The function to call with the response.
   */
  function getAllNodes(callback) {
    requestId++;
    requestCallbacks[requestId] = callback;
    chrome.send('getAllNodes', [requestId]);
  }

  /**
   * Called from C++ with the response to a getAllNodes request.
   *
   * @param {number} id The requestId passed in with the request.
   * @param {Object} response The response to the request.
   */
  function getAllNodesCallback(id, response) {
    requestCallbacks[id](response);
    delete requestCallbacks[id];
  }

  return {
    Timer: Timer,
    dispatchEvent: dispatchEvent,
    // TODO(crbug.com/854268,crbug.com/976249): Use new cr.EventTarget() when
    // the native EventTarget constructor is implemented on iOS (not the case as
    // of 2019-06). In the meantime using a plain div as a workaround, which
    // subclasses EventTarget.
    events: document.createElement('div'),
    getAllNodes: getAllNodes,
    getAllNodesCallback: getAllNodesCallback,
    registerForEvents: registerForEvents,
    registerForPerTypeCounters: registerForPerTypeCounters,
    requestUpdatedAboutInfo: requestUpdatedAboutInfo,
    requestIncludeSpecificsInitialState: requestIncludeSpecificsInitialState,
    requestListOfTypes: requestListOfTypes,
    requestUserEventsVisibility: requestUserEventsVisibility,
    setIncludeSpecifics: setIncludeSpecifics,
    writeUserEvent: writeUserEvent,
    requestStart: requestStart,
    requestStopKeepData: requestStopKeepData,
    requestStopClearData: requestStopClearData,
    triggerRefresh: triggerRefresh,
  };
});
