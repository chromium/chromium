// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

/**
 * A simple timer to measure elapsed time.
 */
export class Timer {
  /* The time that this Timer was created. */
  private start_: number;

  constructor() {
    this.start_ = Date.now();
  }

  /**
   * @return The elapsed seconds since this Timer was created.
   */
  getElapsedSeconds(): number {
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
 * @param includeSpecifics Whether protocol events include specifics.
 */
export function setIncludeSpecifics(includeSpecifics: boolean) {
  chrome.send('setIncludeSpecifics', [includeSpecifics]);
}

/**
 * Sends data to construct a user event that should be committed.
 *
 * @param eventTimeUsec Timestamp for the new event.
 * @param navigationId Timestamp of linked sessions navigation.
 */
export function writeUserEvent(eventTimeUsec: string, navigationId: string) {
  chrome.send('writeUserEvent', [eventTimeUsec, navigationId]);
}

/**
 * Triggers a RequestStart call on the SyncService.
 */
export function requestStart() {
  chrome.send('requestStart');
}

/**
 * Triggers a GetUpdates call for all enabled datatypes.
 */
export function triggerRefresh() {
  chrome.send('triggerRefresh');
}

interface ServerSpecifics {
  autofill: any;
}

type Specifics = ServerSpecifics;

export interface SyncNode {
  BASE_VERSION: string;
  ID: string;
  IS_DIR: boolean;
  METAHANDLE: number;
  NON_UNIQUE_NAME: string;
  PARENT_ID: string;
  UNIQUE_SERVER_TAG: string;
  SERVER_VERSION: string;
  SERVER_VERSION_TIME: string;
  SERVER_SPECIFICS: ServerSpecifics;
  SPECIFICS: Specifics;
  dataType: string;
  positionIndex?: number;
}

export type SyncNodeMap = Array<{type: string, nodes: SyncNode[]}>;

let nodesForTest: SyncNodeMap|null = null;

/**
 * Asks the browser to send us a copy of all existing sync nodes.
 * Will eventually invoke the given callback with the results.
 *
 * @param callback The function to call with the response.
 */
export function getAllNodes(callback: (p: SyncNodeMap) => void) {
  if (nodesForTest) {
    callback(nodesForTest);
    return;
  }
  sendWithPromise('getAllNodes').then(callback);
}

export function setAllNodesForTest(nodes: SyncNodeMap) {
  nodesForTest = nodes;
}
