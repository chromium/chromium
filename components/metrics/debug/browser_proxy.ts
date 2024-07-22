// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

/**
 * @fileoverview A helper object used by the chrome://metrics-internals page to
 * interact with the browser.
 */

/**
 * A pair of strings representing a row in a summary table. For example, |key|
 * could be "Platform", with |value| being "Android".
 */
export interface KeyValue {
  key: string;
  value: string;
}

/**
 * An individual event that occurred on a log. Optionally, this may include a
 * message. For example, for a "Trimmed" event, the message could be "Log size
 * too large".
 */
export interface LogEvent {
  event: string;
  timestampMs: number;
  message?: string;
}

/**
 * A log and its data, including the events that occurred throughout its
 * lifetime. The |type| field is only set for UMA logs (i.e., ongoing,
 * independent, or stability). The |compressed_data| field (i.e., its proto
 * data) is only set when exporting.
 * TODO(crbug.com/40238818): Change name of |type| to something else, since it is
 * confusing and can be mistaken for |logType| in LogData (UMA or UKM).
 */
export interface Log {
  type?: string;
  hash: string;
  timestamp: string;
  data?: string;
  size: number;
  events: LogEvent[];
}

/**
 * A list of logs, as well as their type (UMA or UKM).
 */
export interface LogData {
  logType: string;
  logs: Log[];
}

/**
 * A study or group name along with its hex hash.
 */
export interface HashNamed {
  // `undefined` if we only know the hash.
  name: string|undefined;
  hash: string;
}

/**
 * A Field Trial Group.
 */
export interface Group extends HashNamed {
  forceEnabled: boolean;
  enabled: boolean;
}

/**
 * A Field Trial.
 */
export interface Trial extends HashNamed {
  groups: Group[];
}

/**
 * Maps some hashes to their study/group names.
 */
export interface HashNameMap {
  [hash: string]: string;
}


/**
 * State of all field trials.
 */
export interface FieldTrialState {
  trials: Trial[];
  restartRequired: boolean;
}

export interface MetricsInternalsBrowserProxy {
  /**
   * Gets UMA log data. |includeLogProtoData| determines whether or not the
   * fetched data should also include the protos of the logs.
   */
  getUmaLogData(includeLogProtoData: boolean): Promise<string>;

  /**
   * Fetches a summary of variations info.
   */
  fetchVariationsSummary(): Promise<KeyValue[]>;

  /**
   * Fetches a summary of UMA info.
   */
  fetchUmaSummary(): Promise<KeyValue[]>;

  /**
   * Fetches whether the logs observer being used is owned by the metrics
   * service or is owned by the page.
   */
  isUsingMetricsServiceObserver(): Promise<boolean>;

  /**
   * Overrides the enroll state of a field trial which will be realized after a
   * restart.
   */
  setTrialEnrollState(
      trialHash: string, groupHash: string,
      forceEnable: boolean): Promise<boolean>;

  /**
   * Fetches the current state of the field trials.
   */
  fetchTrialState(): Promise<FieldTrialState>;

  /**
   * Given a trial name, group name, or combination with a [/.-:] separator,
   * returns any name hashes associated with that trial or group.
   */
  lookupTrialOrGroupName(name: string): Promise<HashNameMap>;

  /**
   * Restarts the browser.
   */
  restart(): Promise<void>;
}

export class MetricsInternalsBrowserProxyImpl implements
    MetricsInternalsBrowserProxy {
  getUmaLogData(includeLogProtoData: boolean): Promise<string> {
    return sendWithPromise('fetchUmaLogsData', includeLogProtoData);
  }

  fetchVariationsSummary(): Promise<KeyValue[]> {
    return sendWithPromise('fetchVariationsSummary');
  }

  fetchUmaSummary(): Promise<KeyValue[]> {
    return sendWithPromise('fetchUmaSummary');
  }

  isUsingMetricsServiceObserver(): Promise<boolean> {
    return sendWithPromise('isUsingMetricsServiceObserver');
  }

  setTrialEnrollState(
      trialHash: string, groupHash: string,
      forceEnable: boolean): Promise<boolean> {
    return sendWithPromise(
        'setTrialEnrollState', trialHash, groupHash, forceEnable);
  }

  fetchTrialState(): Promise<FieldTrialState> {
    return sendWithPromise('fetchTrialState');
  }

  lookupTrialOrGroupName(name: string): Promise<HashNameMap> {
    return sendWithPromise('lookupTrialOrGroupName', name);
  }

  restart(): Promise<void> {
    return sendWithPromise('restart');
  }

  static getInstance(): MetricsInternalsBrowserProxy {
    return instance || (instance = new MetricsInternalsBrowserProxyImpl());
  }

  static setInstance(obj: MetricsInternalsBrowserProxy) {
    instance = obj;
  }
}

let instance: MetricsInternalsBrowserProxy|null = null;
