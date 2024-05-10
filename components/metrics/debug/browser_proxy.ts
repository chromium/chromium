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

  static getInstance(): MetricsInternalsBrowserProxy {
    return instance || (instance = new MetricsInternalsBrowserProxyImpl());
  }

  static setInstance(obj: MetricsInternalsBrowserProxy) {
    instance = obj;
  }
}

let instance: MetricsInternalsBrowserProxy|null = null;
